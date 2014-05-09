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

// File: vogl_value.h
#pragma once

#include "vogl_core.h"
#include "vogl_strutils.h"
#include "vogl_dynamic_string.h"
#include "vogl_vec.h"
#include "vogl_hash_map.h"
#include "vogl_data_stream.h"
#include "vogl_json.h"

namespace vogl
{
    // adding: uint8_t, int64_t, uint64_t, double, blob
    enum value_data_type
    {
        cDTInvalid,
        cDTBool,
        cDTInt8,
        cDTUInt8,
        cDTInt16,
        cDTUInt16,
        cDTInt,
        cDTUInt,
        cDTInt64,
        cDTUInt64,
        cDTFloat,
        cDTDouble,
        cDTVoidPtr,
        cDTStringHash,
        cDTFirstDynamic,
        cDTString = cDTFirstDynamic,
        cDTVec3F,
        cDTVec3I,
        cDTBlob,
        cDTJSONDoc,
        cDTTotal
    };

    extern const char *g_value_data_type_strings[cDTTotal + 1];

    value_data_type string_to_value_data_type(const char *pStr);

    class value
    {
        enum
        {
            cFlagsHasUserData = 1
        };

    public:
        inline value()
            : m_uint64(0),
              m_user_data(0),
              m_type(cDTInvalid),
              m_flags(0)
        {
            VOGL_ASSUME(sizeof(float) == sizeof(int));
            VOGL_ASSUME(sizeof(double) == sizeof(uint64_t));
        }

        inline value(const char *pStr)
            : m_pStr(vogl_new(dynamic_string, pStr)), m_user_data(0), m_type(cDTString), m_flags(0)
        {
        }

        inline value(const dynamic_string &str)
            : m_pStr(vogl_new(dynamic_string, str)), m_user_data(0), m_type(cDTString), m_flags(0)
        {
        }

        inline value(const uint8_t *pBuf, uint32_t size)
            : m_pBlob(vogl_new(uint8_vec, size)), m_user_data(0), m_type(cDTBlob), m_flags(0)
        {
            if (size)
                memcpy(m_pBlob->get_ptr(), pBuf, size);
        }

        inline value(const uint8_vec &blob)
            : m_pBlob(vogl_new(uint8_vec, blob)), m_user_data(0), m_type(cDTBlob), m_flags(0)
        {
        }

        inline value(const json_document &doc)
            : m_pJSONDoc(vogl_new(json_document, doc)), m_user_data(0), m_type(cDTJSONDoc), m_flags(0)
        {
        }

        inline explicit value(bool v)
            : m_bool(v), m_user_data(0), m_type(cDTBool), m_flags(0)
        {
        }

        inline explicit value(void *p)
            : m_pPtr(p), m_user_data(0), m_type(cDTVoidPtr), m_flags(0)
        {
        }

        inline value(int8_t v)
            : m_int8(v), m_user_data(0), m_type(cDTInt8), m_flags(0)
        {
        }

        inline value(uint8_t v)
            : m_uint8(v), m_user_data(0), m_type(cDTUInt8), m_flags(0)
        {
        }

        inline value(int16_t v)
            : m_int16(v), m_user_data(0), m_type(cDTInt16), m_flags(0)
        {
        }

        inline value(uint16_t v)
            : m_uint16(v), m_user_data(0), m_type(cDTUInt16), m_flags(0)
        {
        }

        inline value(int v)
            : m_int(v), m_user_data(0), m_type(cDTInt), m_flags(0)
        {
        }

        inline value(uint32_t v)
            : m_uint(v), m_user_data(0), m_type(cDTUInt), m_flags(0)
        {
        }

        inline value(int64_t v)
            : m_int64(v), m_user_data(0), m_type(cDTInt64), m_flags(0)
        {
        }

        inline value(uint64_t v)
            : m_uint64(v), m_user_data(0), m_type(cDTUInt64), m_flags(0)
        {
        }

        inline value(float v)
            : m_float(v), m_user_data(0), m_type(cDTFloat), m_flags(0)
        {
        }

        inline value(double v)
            : m_double(v), m_user_data(0), m_type(cDTDouble), m_flags(0)
        {
        }

        inline value(const vec3F &v)
            : m_pVec3F(vogl_new(vec3F, v)), m_user_data(0), m_type(cDTVec3F), m_flags(0)
        {
        }

        inline value(const vec3I &v)
            : m_pVec3I(vogl_new(vec3I, v)), m_user_data(0), m_type(cDTVec3I), m_flags(0)
        {
        }

        inline value(const value &other)
            : m_type(cDTInvalid),
              m_flags(0)
        {
            *this = other;
        }

        inline value(const string_hash &hash)
            : m_user_data(0), m_type(cDTStringHash), m_flags(0)
        {
            get_string_hash_ref() = hash;
        }

        inline ~value()
        {
            clear_dynamic();
        }

        value &operator=(const value &other);

        bool operator==(const value &other) const;
        bool operator!=(const value &other) const
        {
            return !(*this == other);
        }

        bool operator<(const value &other) const;

        inline value_data_type get_data_type() const
        {
            return m_type;
        }

        inline void clear()
        {
            clear_dynamic();

            m_type = cDTInvalid;
            m_user_data = 0;
            m_uint64 = 0;
            m_flags = 0;
        }

        inline void set_bool(bool v)
        {
            clear_dynamic();
            m_type = cDTBool;
            m_bool = v;
        }

        inline void set_int8(int8_t v)
        {
            clear_dynamic();
            m_type = cDTInt8;
            m_int8 = v;
        }

        inline void set_uint8(uint8_t v)
        {
            clear_dynamic();
            m_type = cDTUInt8;
            m_uint8 = v;
        }

        inline void set_int16(int16_t v)
        {
            clear_dynamic();
            m_type = cDTInt16;
            m_int16 = v;
        }

        inline void set_uint16(uint16_t v)
        {
            clear_dynamic();
            m_type = cDTUInt16;
            m_uint16 = v;
        }

        inline void set_int(int v)
        {
            clear_dynamic();
            m_type = cDTInt;
            m_int = v;
        }

        inline void set_uint(uint32_t v)
        {
            clear_dynamic();
            m_type = cDTUInt;
            m_uint = v;
        }

        inline void set_int64(int64_t v)
        {
            clear_dynamic();
            m_type = cDTInt64;
            m_int64 = v;
        }

        inline void set_uint64(uint64_t v)
        {
            clear_dynamic();
            m_type = cDTUInt64;
            m_uint64 = v;
        }

        inline void set_float(float v)
        {
            clear_dynamic();
            m_type = cDTFloat;
            m_float = v;
        }

        inline void set_double(double v)
        {
            clear_dynamic();
            m_type = cDTDouble;
            m_double = v;
        }

        inline void set_void_ptr(void *p)
        {
            clear_dynamic();
            m_type = cDTVoidPtr;
            m_pPtr = p;
        }

        inline void set_vec(const vec3F &v)
        {
            change_type(cDTVec3F);
            m_pVec3F->set(v);
        }

        inline void set_vec(const vec3I &v)
        {
            change_type(cDTVec3I);
            m_pVec3I->set(v);
        }

        inline void set_string(const char *pStr)
        {
            set_str(pStr);
        }

        inline bool set_blob(const uint8_t *pBlob, uint32_t size)
        {
            change_type(cDTBlob);
            if (!m_pBlob->try_resize(size))
                return false;
            if (size)
                memcpy(m_pBlob->get_ptr(), pBlob, size);
            return true;
        }

        inline bool set_blob(const uint8_vec &v)
        {
            return set_blob(v.get_ptr(), v.size());
        }

        inline void set_blob_take_ownership(uint8_vec &v)
        {
            change_type(cDTBlob);
            m_pBlob->swap(v);
        }

        inline bool set_json_document(const json_document &doc)
        {
            change_type(cDTJSONDoc);
            *m_pJSONDoc = doc;
            return true;
        }

        inline bool set_json_document_take_ownership(json_document &doc)
        {
            change_type(cDTJSONDoc);
            m_pJSONDoc->swap(doc);
            return true;
        }

        inline json_document *init_json_document()
        {
            change_type(cDTJSONDoc);
            return m_pJSONDoc;
        }

        inline void set_string_hash(const string_hash &hash)
        {
            change_type(cDTStringHash);
            m_type = cDTStringHash;
            get_string_hash_ref() = hash;
        }

        inline value &operator=(bool value)
        {
            set_bool(value);
            return *this;
        }
        inline value &operator=(uint8_t value)
        {
            set_uint8(value);
            return *this;
        }
        inline value &operator=(int16_t value)
        {
            set_int16(value);
            return *this;
        }
        inline value &operator=(uint16_t value)
        {
            set_uint16(value);
            return *this;
        }
        inline value &operator=(int value)
        {
            set_int(value);
            return *this;
        }
        inline value &operator=(uint32_t value)
        {
            set_uint(value);
            return *this;
        }
        inline value &operator=(int64_t value)
        {
            set_int64(value);
            return *this;
        }
        inline value &operator=(uint64_t value)
        {
            set_uint64(value);
            return *this;
        }
        inline value &operator=(float value)
        {
            set_float(value);
            return *this;
        }
        inline value &operator=(double value)
        {
            set_double(value);
            return *this;
        }
        inline value &operator=(void *pPtr)
        {
            set_void_ptr(pPtr);
            return *this;
        }
        inline value &operator=(const vec3F &value)
        {
            set_vec(value);
            return *this;
        }
        inline value &operator=(const vec3I &value)
        {
            set_vec(value);
            return *this;
        }
        inline value &operator=(const uint8_vec &value)
        {
            set_blob(value);
            return *this;
        }
        inline value &operator=(const dynamic_string &value)
        {
            set_string(value.get_ptr());
            return *this;
        }
        inline value &operator=(const char *pStr)
        {
            set_string(pStr);
            return *this;
        }
        inline value &operator=(const string_hash &hash)
        {
            set_string_hash(hash);
            return *this;
        }
        inline value &operator=(const json_document &doc)
        {
            set_json_document(doc);
            return *this;
        }

        bool parse(const char *p);

        dynamic_string &get_string(dynamic_string &dst, bool quote_strings = false) const;

        inline dynamic_string get_string(bool quote_strings = false) const
        {
            dynamic_string str;
            get_string(str, quote_strings);
            return str;
        }
        inline dynamic_string *get_string_ptr() const
        {
            return (m_type == cDTString) ? m_pStr : NULL;
        }

        // on failure, the destination val is NOT modified
        bool get_int8_or_fail(int8_t &val, uint32_t component) const;
        bool get_uint8_or_fail(uint8_t &val, uint32_t component) const;
        bool get_int16_or_fail(int16_t &val, uint32_t component) const;
        bool get_uint16_or_fail(uint16_t &val, uint32_t component) const;
        bool get_int_or_fail(int &val, uint32_t component = 0) const;
        bool get_int64_or_fail(int64_t &val, uint32_t component = 0) const;
        bool get_uint_or_fail(uint32_t &val, uint32_t component = 0) const;
        bool get_uint64_or_fail(uint64_t &val, uint32_t component = 0) const;
        bool get_bool_or_fail(bool &val, uint32_t component = 0) const;
        bool get_double_or_fail(double &val, uint32_t component = 0) const;
        bool get_float_or_fail(float &val, uint32_t component = 0) const;
        bool get_vec3F_or_fail(vec3F &val) const;
        bool get_vec3I_or_fail(vec3I &val) const;
        bool get_string_hash_or_fail(string_hash &hash) const;

        inline int8_t get_int8(int8_t def = 0, uint32_t component = 0) const
        {
            int8_t result = def;
            get_int8_or_fail(result, component);
            return result;
        }
        inline int16_t get_int16(int16_t def = 0, uint32_t component = 0) const
        {
            int16_t result = def;
            get_int16_or_fail(result, component);
            return result;
        }
        inline uint8_t get_uint8(uint8_t def = 0, uint32_t component = 0) const
        {
            uint8_t result = def;
            get_uint8_or_fail(result, component);
            return result;
        }
        inline uint16_t get_uint16(uint16_t def = 0, uint32_t component = 0) const
        {
            uint16_t result = def;
            get_uint16_or_fail(result, component);
            return result;
        }
        inline int get_int(int def = 0, uint32_t component = 0) const
        {
            int result = def;
            get_int_or_fail(result, component);
            return result;
        }
        inline int64_t get_int64(int64_t def = 0, uint32_t component = 0) const
        {
            int64_t result = def;
            get_int64_or_fail(result, component);
            return result;
        }
        inline uint32_t get_uint(uint32_t def = 0, uint32_t component = 0) const
        {
            uint32_t result = def;
            get_uint_or_fail(result, component);
            return result;
        }
        inline uint64_t get_uint64(uint64_t def = 0, uint32_t component = 0) const
        {
            uint64_t result = def;
            get_uint64_or_fail(result, component);
            return result;
        }

        inline bool get_bool(bool def = false, uint32_t component = 0) const
        {
            bool result = def;
            get_bool_or_fail(result, component);
            return result;
        }
        inline double get_double(double def = 0.0f, uint32_t component = 0) const
        {
            double result = def;
            get_double_or_fail(result, component);
            return result;
        }
        inline float get_float(float def = 0.0f, uint32_t component = 0) const
        {
            float result = def;
            get_float_or_fail(result, component);
            return result;
        }
        inline vec3F get_vec3F(const vec3F &def = vec3F(0)) const
        {
            vec3F result(def);
            get_vec3F_or_fail(result);
            return result;
        }
        inline vec3I get_vec3I(const vec3I &def = vec3I(0)) const
        {
            vec3I result(def);
            get_vec3I_or_fail(result);
            return result;
        }
        inline string_hash get_string_hash(const string_hash &def = string_hash(0U)) const
        {
            string_hash result(def);
            get_string_hash_or_fail(result);
            return result;
        }

        inline const uint8_vec *get_blob() const
        {
            return (m_type == cDTBlob) ? m_pBlob : NULL;
        }

        inline uint8_vec *get_blob()
        {
            return (m_type == cDTBlob) ? m_pBlob : NULL;
        }

        inline void *get_void_ptr() const
        {
            return (m_type == cDTVoidPtr) ? m_pPtr : NULL;
        }

        inline const json_document *get_json_document() const
        {
            return (m_type == cDTJSONDoc) ? m_pJSONDoc : NULL;
        }

        inline json_document *get_json_document()
        {
            return (m_type == cDTJSONDoc) ? m_pJSONDoc : NULL;
        }

        inline bool set_zero()
        {
            switch (m_type)
            {
                case cDTInvalid:
                {
                    return false;
                }
                case cDTString:
                {
                    m_pStr->empty();
                    break;
                }
                case cDTBool:
                {
                    m_bool = false;
                    break;
                }
                case cDTInt8:
                case cDTUInt8:
                case cDTInt16:
                case cDTUInt16:
                case cDTInt:
                case cDTUInt:
                case cDTInt64:
                case cDTUInt64:
                case cDTFloat:
                case cDTDouble:
                case cDTVoidPtr:
                {
                    m_uint64 = 0;
                    break;
                }
                case cDTVec3F:
                {
                    m_pVec3F->clear();
                    break;
                }
                case cDTVec3I:
                {
                    m_pVec3I->clear();
                    break;
                }
                case cDTBlob:
                {
                    m_pBlob->clear();
                    break;
                }
                case cDTJSONDoc:
                {
                    m_pJSONDoc->clear();
                    break;
                }
                default:
                    VOGL_ASSERT_ALWAYS;
                    break;
            }
            return true;
        }

        inline bool is_valid() const
        {
            return m_type != cDTInvalid;
        }

        inline bool is_dynamic() const
        {
            return m_type >= cDTFirstDynamic;
        }

        inline bool is_vector() const
        {
            switch (m_type)
            {
                case cDTVec3F:
                case cDTVec3I:
                    return true;
                default:
                    break;
            }
            return false;
        }

        inline uint32_t get_num_components() const
        {
            switch (m_type)
            {
                case cDTVec3F:
                case cDTVec3I:
                    return 3;
                case cDTBlob:
                    return m_pBlob->size();
                default:
                    break;
            }
            return 1;
        }

        inline bool is_numeric() const
        {
            switch (m_type)
            {
                case cDTInt8:
                case cDTUInt8:
                case cDTInt:
                case cDTUInt:
                case cDTInt16:
                case cDTUInt16:
                case cDTInt64:
                case cDTUInt64:
                case cDTFloat:
                case cDTDouble:
                case cDTVec3F:
                case cDTVec3I:
                case cDTStringHash:
                    return true;
                default:
                    break;
            }
            return false;
        }

        inline const void *get_data_ptr() const
        {
            if (m_type >= cDTFirstDynamic)
            {
                switch (m_type)
                {
                    case cDTString:
                        return m_pStr->get_ptr();
                    case cDTVec3F:
                        return m_pVec3F;
                    case cDTVec3I:
                        return m_pVec3I;
                    case cDTBlob:
                        return m_pBlob->get_ptr();
                    case cDTJSONDoc:
                        return NULL;
                    default:
                        VOGL_ASSERT_ALWAYS;
                        break;
                }
            }

            return &m_bool;
        }

        inline uint32_t get_data_size_in_bytes() const
        {
            switch (m_type)
            {
                case cDTInvalid:
                    return 0;
                case cDTBool:
                case cDTInt8:
                case cDTUInt8:
                    return sizeof(uint8_t);
                case cDTInt16:
                case cDTUInt16:
                    return sizeof(uint16_t);
                case cDTInt:
                case cDTUInt:
                case cDTFloat:
                case cDTStringHash:
                    return sizeof(uint32_t);
                case cDTInt64:
                case cDTUInt64:
                case cDTDouble:
                    return sizeof(uint64_t);
                case cDTVoidPtr:
                    return sizeof(void *);
                case cDTString:
                    return (m_pStr->get_len() + 1);
                case cDTVec3F:
                    return sizeof(vec3F);
                case cDTVec3I:
                    return sizeof(vec3I);
                case cDTBlob:
                    return m_pBlob->size_in_bytes();
                case cDTJSONDoc:
                    return 0;
                default:
                    VOGL_ASSERT_ALWAYS;
                    break;
            }
            return 0;
        }

        inline bool is_float() const
        {
            switch (m_type)
            {
                case cDTFloat:
                case cDTDouble:
                case cDTVec3F:
                    return true;
                default:
                    break;
            }
            return false;
        }

        inline bool is_integer() const
        {
            switch (m_type)
            {
                case cDTInt8:
                case cDTUInt8:
                case cDTInt16:
                case cDTUInt16:
                case cDTInt:
                case cDTUInt:
                case cDTInt64:
                case cDTUInt64:
                case cDTVec3I:
                case cDTStringHash:
                    return true;
                default:
                    break;
            }
            return false;
        }

        inline bool is_unsigned() const
        {
            switch (m_type)
            {
                case cDTUInt8:
                case cDTUInt16:
                case cDTUInt:
                case cDTUInt64:
                case cDTStringHash:
                    return true;
                default:
                    break;
            }
            return false;
        }

        inline bool is_signed() const
        {
            switch (m_type)
            {
                case cDTInt8:
                case cDTInt:
                case cDTInt16:
                case cDTInt64:
                case cDTFloat:
                case cDTDouble:
                case cDTVec3F:
                case cDTVec3I:
                    return true;
                default:
                    break;
            }
            return false;
        }

        inline bool is_string() const
        {
            return m_type == cDTString;
        }

        inline bool is_blob() const
        {
            return m_type == cDTBlob;
        }

        inline bool is_string_hash() const
        {
            return m_type == cDTStringHash;
        }

        inline bool is_json_document() const
        {
            return m_type == cDTJSONDoc;
        }

        uint32_t get_serialize_size(bool serialize_user_data) const;
        int serialize(void *pBuf, uint32_t buf_size, bool little_endian, bool serialize_user_data) const;
        int deserialize(const void *pBuf, uint32_t buf_size, bool little_endian, bool serialize_user_data);

        inline value &swap(value &other)
        {
            std::swap(m_uint64, other.m_uint64);
            std::swap(m_type, other.m_type);
            std::swap(m_user_data, other.m_user_data);
            std::swap(m_flags, other.m_flags);
            return *this;
        }

        inline bool has_user_data() const
        {
            return (m_flags & cFlagsHasUserData) != 0;
        }
        inline uint32_t get_user_data() const
        {
            return m_user_data;
        }
        inline void set_user_data(uint16_t v)
        {
            m_user_data = v;
            m_flags |= cFlagsHasUserData;
        }
        inline void clear_user_data()
        {
            m_flags &= ~cFlagsHasUserData;
        }

        inline size_t get_hash() const
        {
            if (m_type == cDTJSONDoc)
            {
                uint8_vec data;
                m_pJSONDoc->binary_serialize(data);
                return (((fast_hash(data.get_ptr(), data.size_in_bytes()) + m_type) ^ (m_type << 20)) - bitmix32(m_user_data)) ^ bitmix32(m_flags);
            }
            else
            {
                return (((fast_hash(get_data_ptr(), get_data_size_in_bytes()) + m_type) ^ (m_type << 20)) - bitmix32(m_user_data)) ^ bitmix32(m_flags);
            }
        }

    private:
        inline void clear_dynamic()
        {
            if (m_type >= cDTFirstDynamic)
            {
                if (m_type == cDTVec3F)
                    vogl_delete(m_pVec3F);
                else if (m_type == cDTVec3I)
                    vogl_delete(m_pVec3I);
                else if (m_type == cDTString)
                    vogl_delete(m_pStr);
                else if (m_type == cDTBlob)
                    vogl_delete(m_pBlob);
                else if (m_type == cDTJSONDoc)
                    vogl_delete(m_pJSONDoc);
                else
                {
                    VOGL_ASSERT_ALWAYS;
                }

                m_pPtr = NULL;
                m_type = cDTInvalid;
            }
        }

        inline void change_type(value_data_type type)
        {
            if (type != m_type)
            {
                clear_dynamic();

                m_type = type;
                if (m_type >= cDTFirstDynamic)
                {
                    switch (m_type)
                    {
                        case cDTString:
                            m_pStr = vogl_new(dynamic_string);
                            break;
                        case cDTVec3F:
                            m_pVec3F = vogl_new(vec3F);
                            break;
                        case cDTVec3I:
                            m_pVec3I = vogl_new(vec3I);
                            break;
                        case cDTBlob:
                            m_pBlob = vogl_new(uint8_vec);
                            break;
                        case cDTJSONDoc:
                            m_pJSONDoc = vogl_new(json_document);
                            break;
                        default:
                            break;
                    }
                }
            }
        }

        inline void set_str(const dynamic_string &s)
        {
            if (m_type == cDTString)
                m_pStr->set(s);
            else
            {
                clear_dynamic();

                m_type = cDTString;
                m_pStr = vogl_new(dynamic_string, s);
            }
        }

        inline void set_str(const char *p)
        {
            if (m_type == cDTString)
                m_pStr->set(p);
            else
            {
                clear_dynamic();

                m_type = cDTString;
                m_pStr = vogl_new(dynamic_string, p);
            }
        }

        union
        {
            bool m_bool;

            int8_t m_int8;
            uint8_t m_uint8;

            int16_t m_int16;
            uint16_t m_uint16;

            int m_int;
            uint32_t m_uint;

            int64_t m_int64;
            uint64_t m_uint64;

            float m_float;
            double m_double;
            void *m_pPtr;

            vec3F *m_pVec3F;
            vec3I *m_pVec3I;
            dynamic_string *m_pStr;
            uint8_vec *m_pBlob;
            json_document *m_pJSONDoc;
        };

        const string_hash &get_string_hash_ref() const
        {
            VOGL_ASSUME(sizeof(string_hash) == sizeof(uint32_t));
            return *reinterpret_cast<const string_hash *>(&m_uint);
        }
        string_hash &get_string_hash_ref()
        {
            VOGL_ASSUME(sizeof(string_hash) == sizeof(uint32_t));
            return *reinterpret_cast<string_hash *>(&m_uint);
        }

        // I'm torn about m_user_data/m_flags - may not be useful, but there's room for it due to alignment.
        uint16_t m_user_data;
        value_data_type m_type;
        uint8_t m_flags;
    };

    typedef vogl::vector<value> value_vector;

    template <>
    struct hasher<value>
    {
        inline size_t operator()(const value &key) const
        {
            return key.get_hash();
        }
    };

    typedef hash_map<value, value> value_to_value_hash_map;

    class key_value_map
    {
    public:
        typedef value_to_value_hash_map::iterator iterator;
        typedef value_to_value_hash_map::const_iterator const_iterator;

        inline key_value_map()
        {
        }

        inline ~key_value_map()
        {
            clear();
        }

        inline key_value_map(const key_value_map &other)
        {
            *this = other;
        }

        inline key_value_map &operator=(const key_value_map &rhs)
        {
            if (this == &rhs)
                return *this;
            m_key_values = rhs.m_key_values;
            return *this;
        }

        inline bool operator==(const key_value_map &other) const
        {
            if (m_key_values.size() != other.m_key_values.size())
                return false;

            for (const_iterator it = begin(); it != end(); ++it)
            {
                const_iterator other_it = other.find(it->first);
                if (other_it == other.end())
                    return false;
                if (it->second != other_it->second)
                    return false;
            }

            return true;
        }

        inline bool operator!=(const key_value_map &other) const
        {
            return !(*this == other);
        }

        inline size_t get_hash() const
        {
            size_t h = 0xDEADBEEF ^ bitmix32(get_num_key_values());

            for (const_iterator it = begin(); it != end(); ++it)
            {
                h ^= it->first.get_hash();
                h -= it->second.get_hash();
            }

            return h;
        }

        inline void clear()
        {
            m_key_values.clear();
        }

        inline void reset()
        {
            m_key_values.reset();
        }

        inline void reserve(uint32_t new_capacity)
        {
            m_key_values.reserve(new_capacity);
        }

        inline uint32_t size() const
        {
            return m_key_values.size();
        }

        inline uint64_t get_serialize_size(bool serialize_user_data) const
        {
            uint64_t l = sizeof(uint32_t) * 2;

            for (const_iterator it = begin(); it != end(); ++it)
                l += it->first.get_serialize_size(serialize_user_data) + it->second.get_serialize_size(serialize_user_data);

            return l;
        }

        inline int serialize_to_buffer(void *pBuf, uint32_t buf_size, bool little_endian, bool serialize_user_data) const
        {
            if (buf_size < sizeof(uint32_t) * 2)
                return -1;

            uint32_t buf_left = buf_size;

            void *pSize = reinterpret_cast<uint32_t *>(pBuf);
            if (!utils::write_obj(buf_left, pBuf, buf_left, little_endian))
                return -1;

            uint32_t n = get_num_key_values();
            if (!utils::write_obj(n, pBuf, buf_left, little_endian))
                return -1;

            for (const_iterator it = begin(); it != end(); ++it)
            {
                int num_bytes_written = it->first.serialize(pBuf, buf_left, little_endian, serialize_user_data);
                if (num_bytes_written < 0)
                    return -1;

                pBuf = static_cast<uint8_t *>(pBuf) + num_bytes_written;
                buf_left -= num_bytes_written;

                num_bytes_written = it->second.serialize(pBuf, buf_left, little_endian, serialize_user_data);
                if (num_bytes_written < 0)
                    return -1;

                pBuf = static_cast<uint8_t *>(pBuf) + num_bytes_written;
                buf_left -= num_bytes_written;
            }

            uint32_t total_bytes_written = buf_size - buf_left;

            n = sizeof(uint32_t);
            utils::write_obj(total_bytes_written, pSize, n, little_endian);

            return total_bytes_written;
        }

        inline int deserialize_from_buffer(const void *pBuf, uint32_t buf_size, bool little_endian, bool serialize_user_data)
        {
            m_key_values.reset();

            uint32_t buf_left = buf_size;

            uint32_t len = 0;
            if (!utils::read_obj(len, pBuf, buf_left, little_endian))
                return -1;

            if (len < sizeof(uint32_t) * 2)
                return -1;

            if ((buf_left + sizeof(uint32_t)) < len)
                return -1;

            uint32_t n = 0;
            if (!utils::read_obj(n, pBuf, buf_left, little_endian))
                return -1;

            for (uint32_t i = 0; i < n; ++i)
            {
                value key;
                int num_bytes_read = key.deserialize(pBuf, buf_left, little_endian, serialize_user_data);
                if (num_bytes_read < 0)
                    return -1;

                pBuf = static_cast<const uint8_t *>(pBuf) + num_bytes_read;
                buf_left -= num_bytes_read;

                value val;
                num_bytes_read = val.deserialize(pBuf, buf_left, little_endian, serialize_user_data);
                if (num_bytes_read < 0)
                    return -1;

                pBuf = static_cast<const uint8_t *>(pBuf) + num_bytes_read;
                buf_left -= num_bytes_read;

                if (!insert(key, val).second)
                    return -1;
            }

            uint32_t total_bytes_read = buf_size - buf_left;
            if (total_bytes_read != len)
                return -1;
            return total_bytes_read;
        }

        inline int serialize_to_stream(data_stream &stream, bool little_endian, bool serialize_user_data) const
        {
            uint64_t num_bytes_needed = get_serialize_size(serialize_user_data);
            if (num_bytes_needed > static_cast<uint64_t>(cINT32_MAX))
                return -1;

            uint8_t buf[2048];
            uint8_t *pBuf = buf;
            uint8_vec dyn_buf;

            if (num_bytes_needed > sizeof(buf))
            {
                dyn_buf.resize(static_cast<uint32_t>(num_bytes_needed));
                pBuf = dyn_buf.get_ptr();
            }

            int num_bytes_written = serialize_to_buffer(pBuf, static_cast<uint32_t>(num_bytes_needed), little_endian, serialize_user_data);
            if (num_bytes_written < 0)
                return -1;
            VOGL_ASSERT(num_bytes_needed == static_cast<uint64_t>(num_bytes_written));

            uint32_t num_bytes_streamed = stream.write(pBuf, num_bytes_written);
            if (num_bytes_streamed != static_cast<uint32_t>(num_bytes_written))
                return -1;

            return num_bytes_written;
        }

        inline int deserialize_from_stream(data_stream &stream, bool little_endian, bool serialize_user_data)
        {
            uint32_t num_bytes_read = sizeof(uint32_t) * 2;
            VOGL_NOTE_UNUSED(num_bytes_read);

            uint32_t raw_size;
            if (stream.read(&raw_size, sizeof(raw_size)) != sizeof(raw_size))
                return -1;

            uint32_t size = raw_size;
            if (little_endian != c_vogl_little_endian_platform)
                size = utils::swap32(size);

            if ((size < sizeof(uint32_t) * 2) || (size > static_cast<uint32_t>(cINT32_MAX)))
                return -1;

            uint8_vec buf(size);
            *reinterpret_cast<uint32_t *>(buf.get_ptr()) = raw_size;

            if (stream.read(buf.get_ptr() + sizeof(uint32_t), buf.size() - sizeof(uint32_t)) != (buf.size() - sizeof(uint32_t)))
                return -1;

            return deserialize_from_buffer(buf.get_ptr(), buf.size(), little_endian, serialize_user_data);
        }

        // result.first will be an iterator
        // result.second will be true if the key was inserted, or false if the key already exists (in which case it will not be modified).
        inline value_to_value_hash_map::insert_result insert(const value &key, const value &val)
        {
            return m_key_values.insert(key, val);
        }

        inline iterator find(const value &key)
        {
            return m_key_values.find(key);
        }
        inline const_iterator find(const value &key) const
        {
            return m_key_values.find(key);
        }

        // TODO: add more/easier to use gets
        inline value get_value(const value &key, const value &def) const
        {
            const_iterator it = m_key_values.find(key);
            return (it == m_key_values.end()) ? def : it->second;
        }

        // On failure, the destination val is NOT modified
        inline bool get_string_if_found(const value &key, dynamic_string &dst, bool quote_strings = false) const;
        inline bool get_int_if_found(const value &key, int &val, uint32_t component = 0) const;
        inline bool get_int64_if_found(const value &key, int64_t &val, uint32_t component = 0) const;
        inline bool get_uint_if_found(const value &key, uint32_t &val, uint32_t component = 0) const;
        inline bool get_uint64_if_found(const value &key, uint64_t &val, uint32_t component = 0) const;
        inline bool get_bool_if_found(const value &key, bool &val, uint32_t component = 0) const;
        inline bool get_double_if_found(const value &key, double &val, uint32_t component = 0) const;
        inline bool get_float_if_found(const value &key, float &val, uint32_t component = 0) const;
        inline bool get_vec3F_if_found(const value &key, vec3F &val) const;
        inline bool get_vec3I_if_found(const value &key, vec3I &val) const;
        inline bool get_string_hash_if_found(const value &key, string_hash &val) const;

        inline dynamic_string get_string(const value &key, const char *pDef = "", bool quote_strings = false) const
        {
            dynamic_string result = pDef;
            get_string_if_found(key, result, quote_strings);
            return result;
        }
        inline int get_int(const value &key, int32_t def = 0, uint32_t component = 0) const
        {
            int result = def;
            get_int_if_found(key, result, component);
            return result;
        }
        inline int64_t get_int64(const value &key, int64_t def = 0, uint32_t component = 0) const
        {
            int64_t result = def;
            get_int64_if_found(key, result, component);
            return result;
        }
        inline uint32_t get_uint(const value &key, uint32_t def = 0, uint32_t component = 0) const
        {
            uint32_t result = def;
            get_uint_if_found(key, result, component);
            return result;
        }
        inline uint64_t get_uint64(const value &key, uint64_t def = 0, uint32_t component = 0) const
        {
            uint64_t result = def;
            get_uint64_if_found(key, result, component);
            return result;
        }
        inline bool get_bool(const value &key, bool def = false, uint32_t component = 0) const
        {
            bool result = def;
            get_bool_if_found(key, result, component);
            return result;
        }
        inline float get_float(const value &key, float def = 0.0f, uint32_t component = 0) const
        {
            float result = def;
            get_float_if_found(key, result, component);
            return result;
        }
        inline double get_double(const value &key, double def = 0.0f, uint32_t component = 0) const
        {
            double result = def;
            get_double_if_found(key, result, component);
            return result;
        }
        inline vec3F get_vec3F(const value &key, const vec3F &def = vec3F(0.0f)) const
        {
            vec3F result(def);
            get_vec3F_if_found(key, result);
            return result;
        }
        inline vec3I get_vecI(const value &key, const vec3I &def = vec3I(0)) const
        {
            vec3I result(def);
            get_vec3I_if_found(key, result);
            return result;
        }
        inline string_hash get_string_hash(const value &key, const string_hash &def = string_hash(0U)) const
        {
            string_hash result(def);
            get_string_hash_if_found(key, result);
            return result;
        }

        inline dynamic_string *get_string_ptr(const value &key) const;

        inline const uint8_vec *get_blob(const value &key) const;
        inline uint8_vec *get_blob(const value &key);

        inline const json_document *get_json_document(const value &key) const;
        inline json_document *get_json_document(const value &key);

        inline bool does_key_exist(const value &key) const
        {
            return find(key) != end();
        }

        inline void set(const value &key, const value &val)
        {
            iterator it = find(key);
            if (it == end())
                insert(key, val);
            else
                it->second = val;
        }

        // erase() invalidates any active iterators
        inline bool erase(const value &key)
        {
            return m_key_values.erase(key);
        }

        inline uint32_t get_num_key_values() const
        {
            return m_key_values.size();
        }

        inline iterator begin()
        {
            return m_key_values.begin();
        }
        inline const_iterator begin() const
        {
            return m_key_values.begin();
        }

        inline iterator end()
        {
            return m_key_values.end();
        }
        inline const_iterator end() const
        {
            return m_key_values.end();
        }

        inline value_to_value_hash_map &get_map()
        {
            return m_key_values;
        }
        inline const value_to_value_hash_map &get_map() const
        {
            return m_key_values;
        }

    private:
        value_to_value_hash_map m_key_values;
    };

    inline bool key_value_map::get_string_if_found(const value &key, dynamic_string &dst, bool quote_strings) const
    {
        const_iterator it = m_key_values.find(key);
        if (it == m_key_values.end())
            return false;
        it->second.get_string(dst, quote_strings);
        return true;
    }

    inline bool key_value_map::get_int_if_found(const value &key, int &val, uint32_t component) const
    {
        const_iterator it = m_key_values.find(key);
        if (it == m_key_values.end())
            return false;
        return it->second.get_int_or_fail(val, component);
    }

    inline bool key_value_map::get_int64_if_found(const value &key, int64_t &val, uint32_t component) const
    {
        const_iterator it = m_key_values.find(key);
        if (it == m_key_values.end())
            return false;
        return it->second.get_int64_or_fail(val, component);
    }

    inline bool key_value_map::get_uint_if_found(const value &key, uint32_t &val, uint32_t component) const
    {
        const_iterator it = m_key_values.find(key);
        if (it == m_key_values.end())
            return false;
        return it->second.get_uint_or_fail(val, component);
    }

    inline bool key_value_map::get_uint64_if_found(const value &key, uint64_t &val, uint32_t component) const
    {
        const_iterator it = m_key_values.find(key);
        if (it == m_key_values.end())
            return false;
        return it->second.get_uint64_or_fail(val, component);
    }

    inline bool key_value_map::get_bool_if_found(const value &key, bool &val, uint32_t component) const
    {
        const_iterator it = m_key_values.find(key);
        if (it == m_key_values.end())
            return false;
        return it->second.get_bool_or_fail(val, component);
    }

    inline bool key_value_map::get_double_if_found(const value &key, double &val, uint32_t component) const
    {
        const_iterator it = m_key_values.find(key);
        if (it == m_key_values.end())
            return false;
        return it->second.get_double_or_fail(val, component);
    }

    inline bool key_value_map::get_float_if_found(const value &key, float &val, uint32_t component) const
    {
        const_iterator it = m_key_values.find(key);
        if (it == m_key_values.end())
            return false;
        return it->second.get_float_or_fail(val, component);
    }

    inline bool key_value_map::get_vec3F_if_found(const value &key, vec3F &val) const
    {
        const_iterator it = m_key_values.find(key);
        if (it == m_key_values.end())
            return false;
        return it->second.get_vec3F_or_fail(val);
    }

    inline bool key_value_map::get_vec3I_if_found(const value &key, vec3I &val) const
    {
        const_iterator it = m_key_values.find(key);
        if (it == m_key_values.end())
            return false;
        return it->second.get_vec3I_or_fail(val);
    }

    inline bool key_value_map::get_string_hash_if_found(const value &key, string_hash &val) const
    {
        const_iterator it = m_key_values.find(key);
        if (it == m_key_values.end())
            return false;
        return it->second.get_string_hash_or_fail(val);
    }

    inline dynamic_string *key_value_map::get_string_ptr(const value &key) const
    {
        const_iterator it = m_key_values.find(key);
        if (it == m_key_values.end())
            return NULL;
        return it->second.get_string_ptr();
    }

    inline const uint8_vec *key_value_map::get_blob(const value &key) const
    {
        const_iterator it = m_key_values.find(key);
        if (it == m_key_values.end())
            return NULL;
        return it->second.get_blob();
    }

    inline uint8_vec *key_value_map::get_blob(const value &key)
    {
        iterator it = m_key_values.find(key);
        if (it == m_key_values.end())
            return NULL;
        return it->second.get_blob();
    }

    inline const json_document *key_value_map::get_json_document(const value &key) const
    {
        const_iterator it = m_key_values.find(key);
        if (it == m_key_values.end())
            return NULL;
        return it->second.get_json_document();
    }

    inline json_document *key_value_map::get_json_document(const value &key)
    {
        iterator it = m_key_values.find(key);
        if (it == m_key_values.end())
            return NULL;
        return it->second.get_json_document();
    }

} // namespace vogl
