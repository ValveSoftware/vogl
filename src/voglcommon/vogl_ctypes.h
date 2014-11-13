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
// File: vogl_ctypes.h
#pragma once

//----------------------------------------------------------------------------------------------------------------------
// typedefs/helpers
//----------------------------------------------------------------------------------------------------------------------
typedef void (*vogl_void_func_ptr_t)(void);

template <typename T>
struct gl_ctype_sizeof
{
    enum
    {
        size = sizeof(T)
    };
};
template <>
struct gl_ctype_sizeof<void>
{
    enum
    {
        size = 0
    };
};
template <>
struct gl_ctype_sizeof<_XDisplay>
{
    enum
    {
        size = -1
    };
};
template <>
struct gl_ctype_sizeof<_cl_context>
{
    enum
    {
        size = -1
    };
};
template <>
struct gl_ctype_sizeof<_cl_event>
{
    enum
    {
        size = -1
    };
};

// vogl_trace_ptr_value is large enough to hold any trace pointer, or any trace GLintptr, GLsizeiptr, or ptrdiff_t
typedef uint64_t vogl_trace_ptr_value;

//----------------------------------------------------------------------------------------------------------------------
// ctype enum
//----------------------------------------------------------------------------------------------------------------------
// gl/glx type enums
enum vogl_ctype_t
{
#define DEF_TYPE(name, ctype) name,
#include "gl_glx_cgl_wgl_ctypes.inc"
#undef DEF_TYPE
    VOGL_NUM_CTYPES
};

//----------------------------------------------------------------------------------------------------------------------
// ctype desc struct
//----------------------------------------------------------------------------------------------------------------------
struct vogl_ctype_desc_t
{
    vogl_ctype_t m_ctype;
    vogl_ctype_t m_pointee_ctype; // will be VOGL_VOID if not valid
    const char *m_pName;
    const char *m_pCType;
    int m_size; // will be 0 for void, -1 for opaque types, note this is the size of the ctype in the current process!

    uint32_t m_loki_type_flags; // see loki::TypeTraits::typeFlags

    bool m_is_pointer;
    bool m_is_opaque_pointer; // true if the pointer points to an opaque type of unknown/undefind size
    bool m_is_pointer_diff;   // true if the size of this ctype varies depending on 32/64-bit compilation (intptr_t, or ptrdiff_t)
    bool m_is_opaque_type;    // true if the data type is opaque (not well defined or useful to tracing/replaying, probably varies in size in 32-bit vs 64-bit)
};

//----------------------------------------------------------------------------------------------------------------------
// class vogl_ctypes
//----------------------------------------------------------------------------------------------------------------------
class vogl_ctypes
{
public:
    vogl_ctypes(); // purposely do nothing here, because this object is initialized before global construction time
    vogl_ctypes(uint32_t trace_ptr_size)
    {
        init(trace_ptr_size);
    }

    void init();
    void init(uint32_t trace_ptr_size);

    inline uint32_t get_pointer_size() const
    {
        return m_pointer_size;
    }

    void change_pointer_sizes(uint32_t trace_ptr_size);

    inline const vogl_ctype_desc_t &operator[](vogl_ctype_t ctype) const
    {
        VOGL_ASSERT(ctype < VOGL_NUM_CTYPES);
        return m_vogl_ctype_descs[ctype];
    }
    inline vogl_ctype_desc_t &operator[](vogl_ctype_t ctype)
    {
        VOGL_ASSERT(ctype < VOGL_NUM_CTYPES);
        return m_vogl_ctype_descs[ctype];
    }

    inline const vogl_ctype_desc_t &get_desc(vogl_ctype_t ctype) const
    {
        return (*this)[ctype];
    }
    inline vogl_ctype_desc_t &get_desc(vogl_ctype_t ctype)
    {
        return (*this)[ctype];
    }

    // Returns VOGL_VOID if not valid
    inline vogl_ctype_t get_pointee_ctype(vogl_ctype_t ctype) const
    {
        return (*this)[ctype].m_pointee_ctype;
    }

    inline const char *get_name(vogl_ctype_t ctype) const
    {
        return (*this)[ctype].m_pName;
    }
    inline const char *get_ctype_name(vogl_ctype_t ctype) const
    {
        return (*this)[ctype].m_pCType;
    }
    inline int get_size(vogl_ctype_t ctype) const
    {
        return (*this)[ctype].m_size;
    }
    inline uint32_t get_loki_type_flags(vogl_ctype_t ctype) const
    {
        return (*this)[ctype].m_loki_type_flags;
    }
    inline bool is_pointer(vogl_ctype_t ctype) const
    {
        return (*this)[ctype].m_is_pointer;
    }
    inline bool is_opaque_pointer(vogl_ctype_t ctype) const
    {
        return (*this)[ctype].m_is_opaque_pointer;
    }

    vogl_ctypes &operator=(const vogl_ctypes &other);

private:
    vogl_ctype_desc_t m_vogl_ctype_descs[VOGL_NUM_CTYPES];
    uint32_t m_pointer_size;
};

//----------------------------------------------------------------------------------------------------------------------
//  Global process vogl_ctypes object
//----------------------------------------------------------------------------------------------------------------------
inline vogl_ctypes &get_vogl_process_gl_ctypes()
{
    static vogl_ctypes s_vogl_process_gl_ctypes;
    return s_vogl_process_gl_ctypes;
}

//----------------------------------------------------------------------------------------------------------------------
// Functions
//----------------------------------------------------------------------------------------------------------------------
void vogl_dump_gl_ctypes();
void vogl_init_gl_ctypes();
