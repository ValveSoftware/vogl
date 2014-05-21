/**************************************************************************
 *
 * Copyright 2013-2014 RAD Game Tools and Valve Software
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

// File: vogl_state_vector.cpp
#include "vogl_state_vector.h"
#include "vogl_sort.h"
#include "gl_pname_defs.h"
#include "vogl_growable_array.h"

#include "vogl_blob_manager.h"


#define VOGL_CONTEXT_STATE_DEBUG 0

#if VOGL_CONTEXT_STATE_DEBUG
#pragma message("VOGL_CONTEXT_STATE_DEBUG enabled")
#endif

//----------------------------------------------------------------------------------------------------------------------
// vogl_get_state_type_name
//----------------------------------------------------------------------------------------------------------------------
const char *vogl_get_state_type_name(vogl_state_type s)
{
    VOGL_FUNC_TRACER

    switch (s)
    {
        case cSTInvalid:
            return "invalid";
        case cSTGLboolean:
            return "boolean";
        case cSTGLenum:
            return "GLenum";
        case cSTInt32:
            return "int32_t";
        case cSTUInt32:
            return "uint32_t";
        case cSTInt64:
            return "int64_t";
        case cSTUInt64:
            return "uint64_t";
        case cSTFloat:
            return "float";
        case cSTDouble:
            return "double";
        case cSTPointer:
            return "pointer";
        default:
            VOGL_ASSERT_ALWAYS;
            break;
    }
    return "?";
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_get_state_type_from_name
//----------------------------------------------------------------------------------------------------------------------
vogl_state_type vogl_get_state_type_from_name(const char *pName)
{
    VOGL_FUNC_TRACER

    if (vogl_strcmp(pName, "invalid") == 0)
        return cSTInvalid;
    else if (vogl_strcmp(pName, "boolean") == 0)
        return cSTGLboolean;
    else if (vogl_strcmp(pName, "GLenum") == 0)
        return cSTGLenum;
    else if (vogl_strcmp(pName, "int32_t") == 0)
        return cSTInt32;
    else if (vogl_strcmp(pName, "uint32_t") == 0)
        return cSTUInt32;
    else if (vogl_strcmp(pName, "int64_t") == 0)
        return cSTInt64;
    else if (vogl_strcmp(pName, "uint64_t") == 0)
        return cSTUInt64;
    else if (vogl_strcmp(pName, "float") == 0)
        return cSTFloat;
    else if (vogl_strcmp(pName, "double") == 0)
        return cSTDouble;
    else if (vogl_strcmp(pName, "pointer") == 0)
        return cSTPointer;

    return cSTInvalid;
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_get_state_type_size
//----------------------------------------------------------------------------------------------------------------------
uint32_t vogl_get_state_type_size(vogl_state_type s)
{
    VOGL_FUNC_TRACER

    switch (s)
    {
        case cSTInvalid:
            return 0;
        case cSTGLboolean:
            return sizeof(GLboolean);
        case cSTGLenum:
            return sizeof(GLenum);
        case cSTInt32:
            return sizeof(int32_t);
        case cSTUInt32:
            return sizeof(uint32_t);
        case cSTInt64:
            return sizeof(int64_t);
        case cSTUInt64:
            return sizeof(uint64_t);
        case cSTFloat:
            return sizeof(float);
        case cSTDouble:
            return sizeof(double);
        case cSTPointer:
            return sizeof(uint64_t);
        default:
            VOGL_ASSERT_ALWAYS;
            return 0;
    }
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_state_data::vogl_state_data
//----------------------------------------------------------------------------------------------------------------------
vogl_state_data::vogl_state_data()
    : m_id(0, 0, false),
      m_data_type(cSTInvalid),
      m_num_elements(0)
{
    VOGL_FUNC_TRACER
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_state_data::vogl_state_data
//----------------------------------------------------------------------------------------------------------------------
vogl_state_data::vogl_state_data(GLenum enum_val, uint32_t index, uint32_t n, vogl_state_type data_type, bool indexed_variant)
    : m_id(enum_val, index, indexed_variant),
      m_data_type(data_type),
      m_num_elements(n),
      m_data(n * vogl_get_state_type_size(data_type))
{
    VOGL_FUNC_TRACER
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_state_data::vogl_state_data
//----------------------------------------------------------------------------------------------------------------------
vogl_state_data::vogl_state_data(GLenum enum_val, uint32_t index, const void *pData, uint32_t element_size, bool indexed_variant)
    : m_id(0, 0, false),
      m_data_type(cSTInvalid),
      m_num_elements(0)
{
    VOGL_FUNC_TRACER

    init(enum_val, index, pData, element_size, indexed_variant);
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_state_data::init
//----------------------------------------------------------------------------------------------------------------------
void vogl_state_data::init(GLenum enum_val, uint32_t index, uint32_t n, vogl_state_type data_type, bool indexed_variant)
{
    VOGL_FUNC_TRACER

    m_id.set(enum_val, index, indexed_variant);
    m_data_type = data_type;
    m_num_elements = n;

    uint32_t data_type_size = vogl_get_state_type_size(data_type);

#if VOGL_CONTEXT_STATE_DEBUG
    m_data.resize((n + 1) * data_type_size);
    memset(m_data.get_ptr() + (n * data_type_size), 0xCF, data_type_size);
#else
    m_data.resize(n * data_type_size);
#endif
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_state_data::init
//----------------------------------------------------------------------------------------------------------------------
bool vogl_state_data::init(GLenum enum_val, uint32_t index, const void *pData, uint32_t element_size, bool indexed_variant)
{
    VOGL_FUNC_TRACER

    const char *pName = get_gl_enums().find_name(enum_val);

    int pname_def_index = get_gl_enums().find_pname_def_index(enum_val);
    if (pname_def_index < 0)
    {
        vogl_warning_printf("Unable to find pname def for GL enum %s\n", pName);
        return false;
    }

    const gl_pname_def_t &pname_def = g_gl_pname_defs[pname_def_index];

    VOGL_ASSERT(pname_def.m_gl_enum == enum_val);

    vogl_state_type state_type = static_cast<vogl_state_type>(pname_def.m_type);

    if (state_type == cSTInvalid)
    {
        VOGL_ASSERT_ALWAYS;
        vogl_warning_printf("Can't determine type of GL enum %s\n", pName);
        return false;
    }

    uint32_t n = get_gl_enums().get_pname_count(enum_val);
    if (!n)
    {
        VOGL_ASSERT_ALWAYS;
        return false;
    }

    uint32_t state_type_size = vogl_get_state_type_size(state_type);
    VOGL_ASSERT(state_type_size);

    init(enum_val, index, n, state_type, indexed_variant);

    void *pDst = m_data.get_ptr();
    if (state_type != cSTPointer)
    {
        if (element_size == state_type_size)
            memcpy(pDst, pData, state_type_size * n);
        else if ((state_type == cSTGLboolean) && (element_size == sizeof(GLint)))
        {
            for (uint32_t i = 0; i < n; i++)
                static_cast<GLboolean *>(pDst)[i] = static_cast<const GLint *>(pData)[i];
        }
        else
        {
            VOGL_VERIFY(!"element_size is invalid");
        }
    }
    else
    {
        VOGL_VERIFY(element_size == sizeof(void *));

        // Convert from native 32/64 ptrs to 64-bit values
        const void *const *pSrc_ptrs = static_cast<const void *const *>(pData);
        uint64_t *pDst_u64 = static_cast<uint64_t *>(pDst);
        for (uint32_t i = 0; i < n; i++)
            pDst_u64[i] = reinterpret_cast<uint64_t>(pSrc_ptrs[i]);
    }

    return true;
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_state_data::debug_check
//----------------------------------------------------------------------------------------------------------------------
void vogl_state_data::debug_check()
{
    VOGL_FUNC_TRACER

#if VOGL_CONTEXT_STATE_DEBUG
    if (m_data.size())
    {
        const uint32_t data_type_size = get_data_type_size();
        VOGL_ASSERT((data_type_size) && ((m_data.size() / data_type_size) == (m_num_elements + 1)));
        const uint8_t *p = get_data_ptr<const uint8_t>() + m_num_elements * data_type_size;
        (void)p;
        for (uint32_t i = 0; i < data_type_size; i++)
            VOGL_ASSERT(p[i] == 0xCF);
    }
#endif
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_state_data::swap
//----------------------------------------------------------------------------------------------------------------------
void vogl_state_data::swap(vogl_state_data &other)
{
    VOGL_FUNC_TRACER

    std::swap(m_id, other.m_id);
    std::swap(m_data_type, other.m_data_type);
    std::swap(m_num_elements, other.m_num_elements);
    m_data.swap(other.m_data);
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_state_data::is_equal
//----------------------------------------------------------------------------------------------------------------------
bool vogl_state_data::is_equal(const vogl_state_data &rhs) const
{
    VOGL_FUNC_TRACER

    if (m_id != rhs.m_id)
        return false;
    if (m_data_type != rhs.m_data_type)
        return false;
    if (m_num_elements != rhs.m_num_elements)
        return false;

    uint32_t total_size = get_data_type_size() * m_num_elements;
    if ((m_data.size() < total_size) || (rhs.m_data.size() < total_size))
    {
        VOGL_ASSERT_ALWAYS;
        return false;
    }

    if (memcmp(m_data.get_ptr(), rhs.m_data.get_ptr(), total_size) != 0)
        return false;

    return true;
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_state_data::get_bool
//----------------------------------------------------------------------------------------------------------------------
void vogl_state_data::get_bool(bool *pVals) const
{
    VOGL_FUNC_TRACER

    switch (m_data_type)
    {
        case cSTGLboolean:
        {
            for (uint32_t i = 0; i < m_num_elements; i++)
                pVals[i] = get_element<GLboolean>(i) != GL_FALSE;
            break;
        }
        case cSTGLenum:
        {
            for (uint32_t i = 0; i < m_num_elements; i++)
                pVals[i] = get_element<GLenum>(i) != 0;
            break;
        }
        case cSTInt32:
        {
            for (uint32_t i = 0; i < m_num_elements; i++)
                pVals[i] = get_element<int32_t>(i) != 0;
            break;
        }
        case cSTUInt32:
        {
            for (uint32_t i = 0; i < m_num_elements; i++)
                pVals[i] = get_element<uint32_t>(i) != 0;
            break;
        }
        case cSTInt64:
        {
            for (uint32_t i = 0; i < m_num_elements; i++)
                pVals[i] = get_element<int64_t>(i) != 0;
            break;
        }
        case cSTUInt64:
        {
            for (uint32_t i = 0; i < m_num_elements; i++)
                pVals[i] = get_element<uint64_t>(i) != 0;
            break;
        }
        case cSTFloat:
        {
            for (uint32_t i = 0; i < m_num_elements; i++)
                pVals[i] = get_element<float>(i) != 0.0f;
            break;
        }
        case cSTDouble:
        {
            for (uint32_t i = 0; i < m_num_elements; i++)
                pVals[i] = get_element<double>(i) != 0.0f;
            break;
        }
        case cSTPointer:
        {
            for (uint32_t i = 0; i < m_num_elements; i++)
                pVals[i] = get_element<GLuint64>(i) != 0;
            break;
        }
        case cSTInvalid:
        case cTotalStateTypes:
        {
            VOGL_ASSERT_ALWAYS;
            break;
        }
        default:
        {
            VOGL_ASSERT_ALWAYS;
            break;
        }
    }
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_state_data::get_uint64
// OK, GL itself is not type safe, unsigned vs. signed are mixed everywhere. And the pname table marks a ton of unsigned stuff as signed ints.
// So none of these methods clamp or do anything fancy on ints - I assume you know what you're doing.
//----------------------------------------------------------------------------------------------------------------------
void vogl_state_data::get_uint64(uint64_t *pVals) const
{
    VOGL_FUNC_TRACER

    switch (m_data_type)
    {
        case cSTGLboolean:
        {
            for (uint32_t i = 0; i < m_num_elements; i++)
                pVals[i] = get_element<GLboolean>(i);
            break;
        }
        case cSTGLenum:
        {
            for (uint32_t i = 0; i < m_num_elements; i++)
                pVals[i] = get_element<GLenum>(i);
            break;
        }
        case cSTInt32:
        {
            for (uint32_t i = 0; i < m_num_elements; i++)
                pVals[i] = static_cast<int64_t>(get_element<int32_t>(i));
            break;
        }
        case cSTUInt32:
        {
            for (uint32_t i = 0; i < m_num_elements; i++)
                pVals[i] = get_element<uint32_t>(i);
            break;
        }
        case cSTInt64:
        {
            for (uint32_t i = 0; i < m_num_elements; i++)
                pVals[i] = get_element<int64_t>(i);
            break;
        }
        case cSTPointer:
        case cSTUInt64:
        {
            for (uint32_t i = 0; i < m_num_elements; i++)
                pVals[i] = get_element<uint64_t>(i);
            break;
        }
        case cSTFloat:
        {
            for (uint32_t i = 0; i < m_num_elements; i++)
            {
                double v = get_element<float>(i);
                v = trunc((v < 0.0f) ? (v - .5f) : (v + .5f));
                v = math::clamp<double>(v, static_cast<double>(cINT64_MIN), static_cast<double>(cUINT64_MAX));
                if (v < 0)
                    pVals[i] = static_cast<int64_t>(v);
                else
                    pVals[i] = static_cast<uint64_t>(v);
            }
            break;
        }
        case cSTDouble:
        {
            for (uint32_t i = 0; i < m_num_elements; i++)
            {
                double v = get_element<double>(i);
                v = trunc((v < 0.0f) ? (v - .5f) : (v + .5f));
                v = math::clamp<double>(v, static_cast<double>(cINT64_MIN), static_cast<double>(cUINT64_MAX));
                if (v < 0)
                    pVals[i] = static_cast<int64_t>(v);
                else
                    pVals[i] = static_cast<uint64_t>(v);
            }
            break;
        }
        case cSTInvalid:
        case cTotalStateTypes:
        {
            VOGL_ASSERT_ALWAYS;
            break;
        }
        default:
        {
            VOGL_ASSERT_ALWAYS;
            break;
        }
    }
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_state_data::get_uint
//----------------------------------------------------------------------------------------------------------------------
void vogl_state_data::get_uint(uint32_t *pVals) const
{
    VOGL_FUNC_TRACER

    switch (m_data_type)
    {
        case cSTGLboolean:
        {
            for (uint32_t i = 0; i < m_num_elements; i++)
                pVals[i] = get_element<GLboolean>(i);
            break;
        }
        case cSTGLenum:
        {
            for (uint32_t i = 0; i < m_num_elements; i++)
                pVals[i] = get_element<GLenum>(i);
            break;
        }
        case cSTInt32:
        {
            for (uint32_t i = 0; i < m_num_elements; i++)
                pVals[i] = get_element<int32_t>(i);
            break;
        }
        case cSTUInt32:
        {
            for (uint32_t i = 0; i < m_num_elements; i++)
                pVals[i] = get_element<uint32_t>(i);
            break;
        }
        case cSTInt64:
        {
            for (uint32_t i = 0; i < m_num_elements; i++)
                pVals[i] = static_cast<uint32_t>(get_element<int64_t>(i));
            break;
        }
        case cSTPointer:
        case cSTUInt64:
        {
            for (uint32_t i = 0; i < m_num_elements; i++)
                pVals[i] = static_cast<uint32_t>(get_element<uint64_t>(i));
            break;
        }
        case cSTFloat:
        {
            for (uint32_t i = 0; i < m_num_elements; i++)
            {
                double v = get_element<float>(i);
                v = trunc((v < 0.0f) ? (v - .5f) : (v + .5f));
                v = math::clamp<double>(v, static_cast<double>(cINT32_MIN), static_cast<double>(cUINT32_MAX));
                if (v < 0)
                    pVals[i] = static_cast<int32_t>(v);
                else
                    pVals[i] = static_cast<uint32_t>(v);
            }
            break;
        }
        case cSTDouble:
        {
            for (uint32_t i = 0; i < m_num_elements; i++)
            {
                double v = get_element<double>(i);
                v = trunc((v < 0.0f) ? (v - .5f) : (v + .5f));
                v = math::clamp<double>(v, static_cast<double>(cINT32_MIN), static_cast<double>(cUINT32_MAX));
                if (v < 0)
                    pVals[i] = static_cast<int32_t>(v);
                else
                    pVals[i] = static_cast<uint32_t>(v);
            }
            break;
        }
        case cSTInvalid:
        case cTotalStateTypes:
        {
            VOGL_ASSERT_ALWAYS;
            break;
        }
        default:
        {
            VOGL_ASSERT_ALWAYS;
            break;
        }
    }
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_state_data::get_int64
//----------------------------------------------------------------------------------------------------------------------
void vogl_state_data::get_int64(int64_t *pVals) const
{
    VOGL_FUNC_TRACER

    switch (m_data_type)
    {
        case cSTGLboolean:
        {
            for (uint32_t i = 0; i < m_num_elements; i++)
                pVals[i] = get_element<GLboolean>(i);
            break;
        }
        case cSTGLenum:
        {
            for (uint32_t i = 0; i < m_num_elements; i++)
                pVals[i] = get_element<GLenum>(i);
            break;
        }
        case cSTInt32:
        {
            for (uint32_t i = 0; i < m_num_elements; i++)
                pVals[i] = get_element<int32_t>(i);
            break;
        }
        case cSTUInt32:
        {
            for (uint32_t i = 0; i < m_num_elements; i++)
                pVals[i] = get_element<uint32_t>(i);
            break;
        }
        case cSTInt64:
        {
            for (uint32_t i = 0; i < m_num_elements; i++)
                pVals[i] = get_element<int64_t>(i);
            break;
        }
        case cSTPointer:
        case cSTUInt64:
        {
            for (uint32_t i = 0; i < m_num_elements; i++)
                pVals[i] = get_element<uint64_t>(i);
            break;
        }
        case cSTFloat:
        {
            for (uint32_t i = 0; i < m_num_elements; i++)
            {
                double v = get_element<float>(i);
                v = trunc((v < 0.0f) ? (v - .5f) : (v + .5f));
                v = math::clamp<double>(v, static_cast<double>(cINT64_MIN), static_cast<double>(cUINT64_MAX));
                if (v > cINT64_MAX)
                    pVals[i] = static_cast<uint64_t>(v);
                else
                    pVals[i] = static_cast<int64_t>(v);
            }
            break;
        }
        case cSTDouble:
        {
            for (uint32_t i = 0; i < m_num_elements; i++)
            {
                double v = get_element<double>(i);
                v = trunc((v < 0.0f) ? (v - .5f) : (v + .5f));
                v = math::clamp<double>(v, static_cast<double>(cINT64_MIN), static_cast<double>(cUINT64_MAX));
                if (v > cINT64_MAX)
                    pVals[i] = static_cast<uint64_t>(v);
                else
                    pVals[i] = static_cast<int64_t>(v);
            }
            break;
        }
        case cSTInvalid:
        case cTotalStateTypes:
        {
            VOGL_ASSERT_ALWAYS;
            break;
        }
        default:
        {
            VOGL_ASSERT_ALWAYS;
            break;
        }
    }
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_state_data::get_int
//----------------------------------------------------------------------------------------------------------------------
void vogl_state_data::get_int(int *pVals) const
{
    VOGL_FUNC_TRACER

    switch (m_data_type)
    {
        case cSTGLboolean:
        {
            for (uint32_t i = 0; i < m_num_elements; i++)
                pVals[i] = get_element<GLboolean>(i);
            break;
        }
        case cSTGLenum:
        {
            for (uint32_t i = 0; i < m_num_elements; i++)
                pVals[i] = get_element<GLenum>(i);
            break;
        }
        case cSTInt32:
        {
            for (uint32_t i = 0; i < m_num_elements; i++)
                pVals[i] = get_element<int32_t>(i);
            break;
        }
        case cSTUInt32:
        {
            for (uint32_t i = 0; i < m_num_elements; i++)
                pVals[i] = get_element<uint32_t>(i);
            break;
        }
        case cSTInt64:
        {
            for (uint32_t i = 0; i < m_num_elements; i++)
                pVals[i] = static_cast<int>(get_element<int64_t>(i));
            break;
        }
        case cSTPointer:
        case cSTUInt64:
        {
            for (uint32_t i = 0; i < m_num_elements; i++)
                pVals[i] = static_cast<int>(get_element<uint64_t>(i));
            break;
        }
        case cSTFloat:
        {
            for (uint32_t i = 0; i < m_num_elements; i++)
            {
                double v = get_element<float>(i);
                v = trunc((v < 0.0f) ? (v - .5f) : (v + .5f));
                v = math::clamp<double>(v, static_cast<double>(cINT32_MIN), static_cast<double>(cUINT32_MAX));
                if (v > cINT32_MAX)
                    pVals[i] = static_cast<uint32_t>(v);
                else
                    pVals[i] = static_cast<int32_t>(v);
            }
            break;
        }
        case cSTDouble:
        {
            for (uint32_t i = 0; i < m_num_elements; i++)
            {
                double v = get_element<double>(i);
                v = trunc((v < 0.0f) ? (v - .5f) : (v + .5f));
                v = math::clamp<double>(v, static_cast<double>(cINT32_MIN), static_cast<double>(cUINT32_MAX));
                if (v > cINT32_MAX)
                    pVals[i] = static_cast<uint32_t>(v);
                else
                    pVals[i] = static_cast<int32_t>(v);
            }
            break;
        }
        case cSTInvalid:
        case cTotalStateTypes:
        {
            VOGL_ASSERT_ALWAYS;
            break;
        }
        default:
        {
            VOGL_ASSERT_ALWAYS;
            break;
        }
    }
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_state_data::get_pointer
//----------------------------------------------------------------------------------------------------------------------
void vogl_state_data::get_pointer(void **ppVals) const
{
    VOGL_FUNC_TRACER

    vogl::growable_array<uint64_t, 16> temp(m_num_elements);

    get_uint64(temp.get_ptr());

    for (uint32_t i = 0; i < m_num_elements; i++)
    {
        uint64_t v = temp[i];
        ppVals[i] = *reinterpret_cast<void **>(&v);
    }
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_state_data::get_double
//----------------------------------------------------------------------------------------------------------------------
void vogl_state_data::get_double(double *pVals) const
{
    VOGL_FUNC_TRACER

    switch (m_data_type)
    {
        case cSTGLboolean:
        {
            for (uint32_t i = 0; i < m_num_elements; i++)
                pVals[i] = static_cast<double>(get_element<GLboolean>(i));
            break;
        }
        case cSTGLenum:
        {
            for (uint32_t i = 0; i < m_num_elements; i++)
                pVals[i] = static_cast<double>(get_element<GLenum>(i));
            break;
        }
        case cSTInt32:
        {
            for (uint32_t i = 0; i < m_num_elements; i++)
                pVals[i] = static_cast<double>(get_element<int32_t>(i));
            break;
        }
        case cSTUInt32:
        {
            for (uint32_t i = 0; i < m_num_elements; i++)
                pVals[i] = static_cast<double>(get_element<uint32_t>(i));
            break;
        }
        case cSTInt64:
        {
            for (uint32_t i = 0; i < m_num_elements; i++)
                pVals[i] = static_cast<double>(get_element<int64_t>(i));
            break;
        }
        case cSTPointer:
        case cSTUInt64:
        {
            for (uint32_t i = 0; i < m_num_elements; i++)
                pVals[i] = static_cast<double>(get_element<uint64_t>(i));
            break;
        }
        case cSTFloat:
        {
            for (uint32_t i = 0; i < m_num_elements; i++)
                pVals[i] = static_cast<double>(get_element<float>(i));
            break;
        }
        case cSTDouble:
        {
            for (uint32_t i = 0; i < m_num_elements; i++)
                pVals[i] = get_element<double>(i);
            break;
        }
        case cSTInvalid:
        case cTotalStateTypes:
            break;
        default:
        {
            VOGL_ASSERT_ALWAYS;
            break;
        }
    }
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_state_data::get_float
//----------------------------------------------------------------------------------------------------------------------
void vogl_state_data::get_float(float *pVals) const
{
    VOGL_FUNC_TRACER

    vogl::growable_array<double, 16> temp(m_num_elements);

    get_double(temp.get_ptr());

    for (uint32_t i = 0; i < m_num_elements; i++)
        pVals[i] = static_cast<float>(temp[i]);
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_state_data::serialize
//----------------------------------------------------------------------------------------------------------------------
bool vogl_state_data::serialize(json_node &node) const
{
    VOGL_FUNC_TRACER

    const char *pEnum_name = get_gl_enums().find_name(m_id.m_enum_val);
    dynamic_string enum_name;
    if (pEnum_name)
        enum_name.set(pEnum_name);
    else
        enum_name.format("0x%X", m_id.m_enum_val);

    node.add_key_value("enum", enum_name.get_ptr());
    node.add_key_value("index", m_id.m_index);
    node.add_key_value("indexed_variant", m_id.m_indexed_variant);

    node.add_key_value("data_type", vogl_get_state_type_name(m_data_type));
    json_node &values_node = node.add_array("values");

    // >=, not ==, in case of VOGL_CONTEXT_STATE_DEBUG
    VOGL_ASSERT(m_data.size_in_bytes() >= m_num_elements * get_data_type_size());

    for (uint32_t i = 0; i < m_num_elements; i++)
    {
        const void *pElement = m_data.get_ptr() + i * get_data_type_size();

        switch (m_data_type)
        {
            case cSTGLboolean:
            {
                const GLboolean *pVal = reinterpret_cast<const GLboolean *>(pElement);
                values_node.add_value(*pVal ? true : false);
                break;
            }
            case cSTGLenum:
            {
                const GLenum *pVal = reinterpret_cast<const GLenum *>(pElement);

                const char *pValue_enum_name = get_gl_enums().find_name(*pVal);
                dynamic_string value_enum_name;
                if (pValue_enum_name)
                    value_enum_name.set(pValue_enum_name);
                else
                    value_enum_name.format("0x%X", *pVal);

                values_node.add_value(value_enum_name);
                break;
            }
            case cSTInt32:
            {
                const int32_t *pVal = reinterpret_cast<const int32_t *>(pElement);
                values_node.add_value(*pVal);
                break;
            }
            case cSTUInt32:
            {
                const uint32_t *pVal = reinterpret_cast<const uint32_t *>(pElement);
                values_node.add_value(*pVal);
                break;
            }
            case cSTInt64:
            {
                const int64_t *pVal = reinterpret_cast<const int64_t *>(pElement);
                values_node.add_value(*pVal);
                break;
            }
            case cSTFloat:
            {
                const float *pVal = reinterpret_cast<const float *>(pElement);
                values_node.add_value(*pVal);
                break;
            }
            case cSTDouble:
            {
                const double *pVal = reinterpret_cast<const double *>(pElement);
                values_node.add_value(*pVal);
                break;
            }
            case cSTPointer:
            case cSTUInt64:
            {
                char buf[256];
                vogl_sprintf_s(buf, sizeof(buf), "0x%" PRIX64, *reinterpret_cast<const uint64_t *>(pElement));
                values_node.add_value(buf);
                break;
            }
            default:
            {
                VOGL_ASSERT_ALWAYS;
                values_node.add_value(0);
                break;
            }
        }
    }

    return true;
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_state_data::deserialize
//----------------------------------------------------------------------------------------------------------------------
bool vogl_state_data::deserialize(const json_node &node)
{
    VOGL_FUNC_TRACER

    dynamic_string key_str(node.value_as_string("enum"));

    uint64_t gl_enum = 0;
    if (key_str.begins_with("0x"))
        gl_enum = string_to_uint64(key_str.get_ptr(), gl_enums::cUnknownEnum);
    else
        gl_enum = get_gl_enums().find_enum(key_str.get_ptr());

    if ((gl_enum == gl_enums::cUnknownEnum) || (gl_enum > cUINT32_MAX))
    {
        vogl_warning_printf("Unknown/invalid GL enum \"%s\"\n", key_str.get_ptr());
        return false;
    }

    int index = node.value_as_int("index", -1);
    if (index < 0)
    {
        vogl_warning_printf("Expected index for GL enum \"%s\"\n", key_str.get_ptr());
        return false;
    }

    bool indexed_variant = node.value_as_bool("indexed_variant", false);

    dynamic_string state_type_str(node.value_as_string("data_type"));

    vogl_state_type state_type = vogl_get_state_type_from_name(state_type_str.get_ptr());
    if (state_type == cSTInvalid)
    {
        vogl_warning_printf("Unknown state_type for GL enum \"%s\"\n", key_str.get_ptr());
        return false;
    }

    const json_node *pValues_array = node.find_child_array("values");
    if (!pValues_array)
    {
        vogl_warning_printf("Can't find values array for GL enum \"%s\"\n", key_str.get_ptr());
        return false;
    }

    uint32_t num_elements = pValues_array->size();
    uint32_t state_type_size = vogl_get_state_type_size(state_type);

    vogl_state_data state;
    init(static_cast<GLenum>(gl_enum), index, num_elements, state_type, indexed_variant);

    for (uint32_t element_index = 0; element_index < num_elements; element_index++)
    {
        void *pElement = m_data.get_ptr() + element_index * state_type_size;

        switch (state_type)
        {
            case cSTGLboolean:
            {
                GLboolean *pVal = reinterpret_cast<GLboolean *>(pElement);
                *pVal = pValues_array->value_as_bool(element_index);
                break;
            }
            case cSTGLenum:
            {
                const char *pStr = pValues_array->value_as_string_ptr(element_index);
                if (!pStr)
                {
                    vogl_warning_printf("Failed converting value %u of GL enum \"%s\"\n", element_index, key_str.get_ptr());
                    return false;
                }

                uint64_t gl_enum_val = 0;
                if ((pStr[0] == '0') && (pStr[1] == 'x'))
                    gl_enum_val = string_to_uint64(pStr, gl_enums::cUnknownEnum);
                else
                    gl_enum_val = get_gl_enums().find_enum(dynamic_string(pStr));

                if ((gl_enum_val == gl_enums::cUnknownEnum) || (gl_enum_val > cUINT32_MAX))
                {
                    vogl_error_printf("Unknown/invalid GL enum \"%s\"\n", pStr);
                    return false;
                }

                GLenum *pVal = reinterpret_cast<GLenum *>(pElement);
                *pVal = static_cast<GLenum>(gl_enum_val);

                break;
            }
            case cSTInt32:
            {
                int32_t *pVal = reinterpret_cast<int32_t *>(pElement);
                *pVal = pValues_array->value_as_int(element_index);
                break;
            }
            case cSTUInt32:
            {
                uint32_t *pVal = reinterpret_cast<uint32_t *>(pElement);

                int64_t v = pValues_array->value_as_int64(element_index);
                *pVal = static_cast<uint32_t>(math::clamp<int64_t>(v, 0, cUINT32_MAX));

                break;
            }
            case cSTInt64:
            {
                int64_t *pVal = reinterpret_cast<int64_t *>(pElement);
                *pVal = pValues_array->value_as_int64(element_index);
                break;
            }
            case cSTFloat:
            {
                float *pVal = reinterpret_cast<float *>(pElement);
                *pVal = pValues_array->value_as_float(element_index);
                break;
            }
            case cSTDouble:
            {
                double *pVal = reinterpret_cast<double *>(pElement);
                *pVal = pValues_array->value_as_double(element_index);
                break;
            }
            case cSTPointer:
            case cSTUInt64:
            {
                const char *pStr = pValues_array->value_as_string_ptr(element_index);
                if (!pStr)
                {
                    vogl_warning_printf("Failed converting value %u of GL enum \"%s\"\n", element_index, key_str.get_ptr());
                    return false;
                }

                uint64_t *pVal = reinterpret_cast<uint64_t *>(pElement);

                uint64_t val = 0;
                const char *pBuf = pStr;
                string_ptr_to_uint64(pBuf, val);
                *pVal = val;

                break;
            }
            default:
            {
                VOGL_ASSERT_ALWAYS;
                break;
            }
        }
    }

    debug_check();

    return true;
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_state_vector::vogl_state_vector
//----------------------------------------------------------------------------------------------------------------------
vogl_state_vector::vogl_state_vector()
{
    VOGL_FUNC_TRACER
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_state_vector::clear
//----------------------------------------------------------------------------------------------------------------------
void vogl_state_vector::clear()
{
    VOGL_FUNC_TRACER

    m_states.clear();
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_state_vector::find
//----------------------------------------------------------------------------------------------------------------------
const vogl_state_data *vogl_state_vector::find(GLenum enum_val, uint32_t index, bool indexed_variant) const
{
    VOGL_FUNC_TRACER

    return m_states.find_value(vogl_state_id(enum_val, index, indexed_variant));
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_state_vector::serialize
//----------------------------------------------------------------------------------------------------------------------
bool vogl_state_vector::serialize(json_node &node, vogl_blob_manager &blob_manager) const
{
    VOGL_FUNC_TRACER

    VOGL_NOTE_UNUSED(blob_manager);

    json_node &node_array = node.add_array("states");

    for (state_map::const_iterator it = m_states.begin(); it != m_states.end(); ++it)
    {
        const vogl_state_data &data = it->second;

        if (!data.serialize(node_array.add_object()))
            return false;
    }

    return true;
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_state_vector::deserialize
//----------------------------------------------------------------------------------------------------------------------
bool vogl_state_vector::deserialize(const json_node &node, const vogl_blob_manager &blob_manager)
{
    VOGL_FUNC_TRACER

    VOGL_NOTE_UNUSED(blob_manager);

    clear();

    const json_node *pNode_array = node.find_child_array("states");
    if (!pNode_array)
        return false;

    for (uint32_t value_index = 0; value_index < pNode_array->size(); value_index++)
    {
        const json_node *pState_node = pNode_array->get_child(value_index);
        if (!pState_node)
            return false;

        vogl_state_data state;
        if (!state.deserialize(*pState_node))
            continue;

        if (!insert(state))
        {
            vogl_warning_printf("vogl_state_vector::deserialize: Ignoring duplicate state 0x%X index %u\n", state.get_enum_val(), state.get_index());
        }
    }

    return true;
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_state_vector::deserialize
//----------------------------------------------------------------------------------------------------------------------
bool vogl_state_vector::deserialize(const char *pChild_name, const json_node &parent_node, const vogl_blob_manager &blob_manager)
{
    VOGL_FUNC_TRACER

    clear();

    const json_node *pChild_node = parent_node.find_child_object(pChild_name);
    if (!pChild_node)
        return false;

    return deserialize(*pChild_node, blob_manager);
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_state_vector::operator==
//----------------------------------------------------------------------------------------------------------------------
bool vogl_state_vector::operator==(const vogl_state_vector &rhs) const
{
    VOGL_FUNC_TRACER

    if (m_states.size() != rhs.m_states.size())
        return false;

    VOGL_ASSERT(m_states.debug_check());
    VOGL_ASSERT(rhs.m_states.debug_check());

    const_iterator lhs_it(begin());
    const_iterator rhs_it(rhs.begin());
    for (; lhs_it != end(); ++lhs_it, ++rhs_it)
    {
        if (rhs_it == rhs.end())
        {
            VOGL_ASSERT_ALWAYS;
            return false;
        }

        const vogl_state_data &lhs = lhs_it->second;
        const vogl_state_data &rhs2 = rhs_it->second;

        if (lhs.get_id() != rhs2.get_id())
            return false;

        if (lhs.get_enum_val() == GL_TIMESTAMP)
            continue;

        if (!lhs.is_equal(rhs2))
            return false;
    }

    return true;
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_state_vector::insert
//----------------------------------------------------------------------------------------------------------------------
bool vogl_state_vector::insert(GLenum enum_val, uint32_t index, const void *pData, uint32_t element_size, bool indexed_variant)
{
    VOGL_FUNC_TRACER

    vogl_state_data state_data;
    if (!state_data.init(enum_val, index, pData, element_size, indexed_variant))
        return false;

    return insert(state_data);
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_state_vector::insert
//----------------------------------------------------------------------------------------------------------------------
bool vogl_state_vector::insert(const vogl_state_data &state_data)
{
    VOGL_FUNC_TRACER

    return m_states.insert(state_data.get_id(), state_data).second;
}
