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

// File: vogl_ctypes.cpp

/*
  Linux x86 is ILP32, x64 is LP64:

                ILP32       LP64       LLP64     ILP64
    char            8       8          8         8
    short          16       16         16        16
    int            32       32         32        64
    long           32       64         32        64
    long long      64       64         64        64
    pointer        32       64         64        64
 */

#include "vogl_common.h"
#include "vogl_console.h"
#include "vogl_command_line_params.h"

// loki
#include "TypeTraits.h"

//----------------------------------------------------------------------------------------------------------------------
// global array mapping gl/glx type enums to descriptions, valid for the current process (not necessarily the trace)
//----------------------------------------------------------------------------------------------------------------------
static vogl_ctype_desc_t g_vogl_ctype_descs[] =
    {
#define DEF_TYPE(name, ctype)                                                        \
    {                                                                                \
        name, VOGL_INVALID_CTYPE, #name, #ctype, 0, false, false, false, false, false \
    }                                                                                \
    ,
#include "gl_glx_cgl_wgl_ctypes.inc"
#undef DEF_TYPE
    };

//----------------------------------------------------------------------------------------------------------------------
// vogl_ctypes::vogl_ctypes
//----------------------------------------------------------------------------------------------------------------------
vogl_ctypes::vogl_ctypes()
{
    // purposely do nothing here, because this object is initialized before global construction time
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_ctypes::init
//----------------------------------------------------------------------------------------------------------------------
void vogl_ctypes::init()
{
    m_pointer_size = sizeof(void *);

    memcpy(m_vogl_ctype_descs, g_vogl_ctype_descs, sizeof(m_vogl_ctype_descs));

    // sanity checks
    VOGL_ASSERT(m_vogl_ctype_descs[VOGL_GLVOID_PTR].m_is_pointer == true);
    VOGL_ASSERT(m_vogl_ctype_descs[VOGL_GLVOID_PTR].m_is_opaque_pointer == true);
    VOGL_ASSERT(m_vogl_ctype_descs[VOGL_GLVOID_PTR].m_size == static_cast<int>(m_pointer_size));
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_ctypes::init
//----------------------------------------------------------------------------------------------------------------------
void vogl_ctypes::init(uint32_t trace_ptr_size)
{
    m_pointer_size = trace_ptr_size;

    change_pointer_sizes(trace_ptr_size);
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_ctypes::change_pointer_sizes
// This code attempts to programmatically change the ctype sizes depending on the platform (x86 vs. x64).
//----------------------------------------------------------------------------------------------------------------------
void vogl_ctypes::change_pointer_sizes(uint32_t trace_ptr_size)
{
    VOGL_ASSUME(sizeof(intptr_t) == sizeof(void *));
    VOGL_ASSUME(sizeof(ptrdiff_t) == sizeof(void *));
    VOGL_ASSUME(sizeof(long long) == sizeof(uint64_t));

    VOGL_ASSERT((trace_ptr_size == sizeof(uint32_t)) || (trace_ptr_size == sizeof(uint64_t)));

    m_pointer_size = trace_ptr_size;

    memcpy(m_vogl_ctype_descs, g_vogl_ctype_descs, sizeof(m_vogl_ctype_descs));

    for (uint32_t ctype_iter = 0; ctype_iter < VOGL_NUM_CTYPES; ctype_iter++)
    {
        vogl_ctype_desc_t &desc = m_vogl_ctype_descs[ctype_iter];
        if ((desc.m_is_pointer_diff) || (desc.m_loki_type_flags & LOKI_TYPE_BITMASK(LOKI_IS_POINTER)))
        {
            desc.m_size = trace_ptr_size;
        }
        else
        {
            // sanity check
            VOGL_ASSERT(!desc.m_is_pointer && !desc.m_is_opaque_pointer);
        }
    }

// Mark up long and XID types, which are unsigned longs
#if defined(PLATFORM_LINUX) || defined(PLATFORM_OSX)
    #define DEF_LONG_TYPE(x) m_vogl_ctype_descs[x].m_size = trace_ptr_size;
#elif defined(PLATFORM_WINDOWS)
    #define DEF_LONG_TYPE(x) m_vogl_ctype_descs[x].m_size = sizeof(long);
    #pragma message("Need to determine the right mechanism to roundtrip data through traces.")
#else
    #error "Need to determine the right size for this platform."
#endif

    DEF_LONG_TYPE(VOGL_LONG);
    DEF_LONG_TYPE(VOGL_UNSIGNED_LONG);
    DEF_LONG_TYPE(VOGL_GLXPIXMAP);
    DEF_LONG_TYPE(VOGL_GLXDRAWABLE);
    DEF_LONG_TYPE(VOGL_GLXCONTEXTID);
    DEF_LONG_TYPE(VOGL_GLXWINDOW);
    DEF_LONG_TYPE(VOGL_GLXPBUFFER);
    DEF_LONG_TYPE(VOGL_GLXVIDEOCAPTUREDEVICENV);
    DEF_LONG_TYPE(VOGL_COLORMAP);
    DEF_LONG_TYPE(VOGL_WINDOW);
    DEF_LONG_TYPE(VOGL_FONT);
    //DEF_LONG_TYPE(VOGL_DRAWABLE);
    DEF_LONG_TYPE(VOGL_PIXMAP);
//DEF_LONG_TYPE(VOGL_CURSOR);
#undef DEF_LONG_TYPE

    // Special case the XVisualInfo struct because its size differs between 32-bit and 64-bit - argh this sucks and we need a MUCH cleaner way of handling this.
    // In practice this only fixes an assertion, because the packets always describe the exact length of objects.
    m_vogl_ctype_descs[VOGL_XVISUALINFO].m_size = (trace_ptr_size == 8) ? 64 : 40;

#if !defined(NDEBUG) || VOGL_ENABLE_ASSERTIONS_IN_ALL_BUILDS
    // sanity check
    for (uint32_t ctype_iter = 0; ctype_iter < VOGL_NUM_CTYPES; ctype_iter++)
    {
        vogl_ctype_desc_t &desc = m_vogl_ctype_descs[ctype_iter];
        if ((!desc.m_is_pointer) && (!desc.m_is_pointer_diff))
        {
            VOGL_ASSERT(!desc.m_is_opaque_pointer);
            continue;
        }

        VOGL_ASSERT(desc.m_size == static_cast<int>(trace_ptr_size));
    }
#endif
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_ctypes::operator=
//----------------------------------------------------------------------------------------------------------------------
vogl_ctypes &vogl_ctypes::operator=(const vogl_ctypes &other)
{
    if (this == &other)
        return *this;
    memcpy(this, &other, sizeof(*this));
    return *this;
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_dump_gl_ctypes
//----------------------------------------------------------------------------------------------------------------------
void vogl_dump_gl_ctypes()
{
    printf("vogl_ctypes:\n");
    for (int i = 0; i < VOGL_NUM_CTYPES; i++)
    {
        const vogl_ctype_desc_t &desc = g_vogl_ctype_descs[i];

        printf("%u: %s \"%s\" is_pointer: %u size: %i, pointee type: %s, opaque pointer: %u, pointer diff type: %u, type flags: 0x%08X\n",
               i, desc.m_pName, desc.m_pCType,
               desc.m_is_pointer, desc.m_size,
               (desc.m_pointee_ctype != VOGL_INVALID_CTYPE) ? g_vogl_ctype_descs[desc.m_pointee_ctype].m_pName : "N/A",
               desc.m_is_opaque_pointer,
               desc.m_is_pointer_diff,
               desc.m_loki_type_flags);
    }
    printf("\n");
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_define_special_ctypes
// define opaque and pointer diff types
// Note: Opaque types may have different sizes in 32-bit vs 64-bit. We don't even try to get their ctype desc sizes
// right, because they can change.
//----------------------------------------------------------------------------------------------------------------------
static void vogl_define_special_ctypes()
{
// define opaque and opaque ptr types
#define DEF_OPAQUE_TYPE(type)                                 \
    VOGL_ASSERT(!g_vogl_ctype_descs[type].m_is_pointer);     \
    VOGL_ASSERT(!g_vogl_ctype_descs[type].m_is_opaque_type); \
    g_vogl_ctype_descs[type].m_is_opaque_type = true;

#define DEF_OPAQUE_PTR_TYPE(type)                                                               \
    {                                                                                           \
        VOGL_ASSERT(g_vogl_ctype_descs[type].m_is_pointer);                                    \
        VOGL_ASSERT(!g_vogl_ctype_descs[type].m_is_opaque_pointer);                            \
        g_vogl_ctype_descs[type].m_is_opaque_pointer = true;                                     \
        if (g_vogl_ctype_descs[type].m_pointee_ctype != VOGL_VOID)                                \
            g_vogl_ctype_descs[g_vogl_ctype_descs[type].m_pointee_ctype].m_is_opaque_type = true; \
    }

    DEF_OPAQUE_PTR_TYPE(VOGL_CONST_VOID_PTR)
    DEF_OPAQUE_PTR_TYPE(VOGL_GLDEBUGPROC)
    DEF_OPAQUE_PTR_TYPE(VOGL_GLDEBUGPROCAMD)
    DEF_OPAQUE_PTR_TYPE(VOGL_GLDEBUGPROCARB)
    DEF_OPAQUE_PTR_TYPE(VOGL_GLVOID_PTR)
    DEF_OPAQUE_PTR_TYPE(VOGL_GLXEXTFUNCPTR)
    DEF_OPAQUE_PTR_TYPE(VOGL_GLSYNC)
    DEF_OPAQUE_PTR_TYPE(VOGL_GLXCONTEXT)
    DEF_OPAQUE_PTR_TYPE(VOGL_CONST_GLXCONTEXT)
    DEF_OPAQUE_PTR_TYPE(VOGL_GLXFBCONFIG)
    DEF_OPAQUE_PTR_TYPE(VOGL_DISPLAY_PTR)
    DEF_OPAQUE_PTR_TYPE(VOGL_STRUCT_CLCONTEXT_PTR)
    DEF_OPAQUE_PTR_TYPE(VOGL_STRUCT_CLEVENT_PTR)

    DEF_OPAQUE_PTR_TYPE(VOGL_HANDLE)
    DEF_OPAQUE_PTR_TYPE(VOGL_HDC)
    DEF_OPAQUE_PTR_TYPE(VOGL_HGLRC)
    DEF_OPAQUE_PTR_TYPE(VOGL_HGPUNV)
    DEF_OPAQUE_PTR_TYPE(VOGL_HPBUFFERARB)
    DEF_OPAQUE_PTR_TYPE(VOGL_HPBUFFEREXT)
    DEF_OPAQUE_PTR_TYPE(VOGL_HPVIDEODEV)
    DEF_OPAQUE_PTR_TYPE(VOGL_HVIDEOINPUTDEVICENV)
    DEF_OPAQUE_PTR_TYPE(VOGL_HVIDEOOUTPUTDEVICENV)
    DEF_OPAQUE_PTR_TYPE(VOGL_LPVOID)
    DEF_OPAQUE_PTR_TYPE(VOGL_PGPUDEVICE)
    DEF_OPAQUE_PTR_TYPE(VOGL_PROC)


    DEF_OPAQUE_TYPE(VOGL_VOID)
    DEF_OPAQUE_TYPE(VOGL_WINDOW)
    DEF_OPAQUE_TYPE(VOGL_XVISUALINFO)
    DEF_OPAQUE_TYPE(VOGL_COLORMAP) // rg - added 11/14/13

    // Consider adding all non-this-platform types as opaque types for the other platforms?
    #if (!VOGL_PLATFORM_HAS_GLX)
        DEF_OPAQUE_TYPE(VOGL_FONT)
        DEF_OPAQUE_TYPE(VOGL_GLXCONTEXTID);
        DEF_OPAQUE_TYPE(VOGL_GLXDRAWABLE);
        DEF_OPAQUE_TYPE(VOGL_GLXPBUFFER);
        DEF_OPAQUE_TYPE(VOGL_GLXPIXMAP);
        DEF_OPAQUE_TYPE(VOGL_GLXVIDEOCAPTUREDEVICENV);
        DEF_OPAQUE_TYPE(VOGL_GLXWINDOW);
        DEF_OPAQUE_TYPE(VOGL_PIXMAP);

    #endif


#undef DEF_OPAQUE_PTR_TYPE
#undef DEF_OPAQUE_TYPE

// Define pointer diff types
#define DEF_POINTER_DIFF_TYPE(x) g_vogl_ctype_descs[x].m_is_pointer_diff = true;
    DEF_POINTER_DIFF_TYPE(VOGL_GLVDPAUSURFACENV)
    DEF_POINTER_DIFF_TYPE(VOGL_GLINTPTR)
    DEF_POINTER_DIFF_TYPE(VOGL_GLINTPTRARB)
    DEF_POINTER_DIFF_TYPE(VOGL_GLSIZEIPTR)
    DEF_POINTER_DIFF_TYPE(VOGL_GLSIZEIPTRARB)
#undef DEF_POINTER_DIFF_TYPE
}

//----------------------------------------------------------------------------------------------------------------------
// vogl_init_gl_ctypes
//----------------------------------------------------------------------------------------------------------------------
void vogl_init_gl_ctypes()
{
// Define ctypes
#define DEF_TYPE(name, ctype)                             \
    {                                                     \
        vogl_ctype_desc_t &desc = g_vogl_ctype_descs[name]; \
        typedef Loki::TypeTraits<ctype> ctype_traits;     \
        desc.m_size = gl_ctype_sizeof<ctype>::size;       \
        desc.m_is_pointer = ctype_traits::isPointer;      \
        desc.m_loki_type_flags = ctype_traits::typeFlags; \
    }
#include "gl_glx_cgl_wgl_ctypes.inc"
#undef DEF_TYPE

// Define ptr to pointee mappings
#define DEF_PTR_TO_POINTEE_TYPE(ptr_ctype, pointee_ctype) g_vogl_ctype_descs[ptr_ctype].m_pointee_ctype = pointee_ctype;
#include "gl_glx_cgl_wgl_ctypes_ptr_to_pointee.inc"
#undef DEF_PTR_TO_POINTEE_TYPE

    vogl_define_special_ctypes();

    for (int i = 0; i < VOGL_NUM_CTYPES; i++)
    {
        const vogl_ctype_desc_t &desc = g_vogl_ctype_descs[i];
        bool has_pointee_ctype = (desc.m_pointee_ctype != VOGL_INVALID_CTYPE);

        if (desc.m_is_pointer)
        {
            if (!has_pointee_ctype)
            {
                if (!desc.m_is_opaque_pointer)
                    vogl_warning_printf("ctype %s is a non-opaque pointer, but does not have a valid pointee ctype!\n", desc.m_pName);
            }
            else
            {
                const vogl_ctype_desc_t &pointee_desc = g_vogl_ctype_descs[desc.m_pointee_ctype];

                if ((desc.m_is_opaque_pointer) && (pointee_desc.m_size > 0))
                {
                    vogl_warning_printf("ctype %s is a pointer with a valid pointee ctype %s size %i, but has been marked as an opaque ptr!\n", desc.m_pName, g_vogl_ctype_descs[desc.m_pointee_ctype].m_pName, pointee_desc.m_size);
                }

                if ((!pointee_desc.m_size) && (desc.m_pointee_ctype != VOGL_GLVOID) && (desc.m_pointee_ctype != VOGL_VOID))
                    vogl_warning_printf("ctype %s is a pointer with a valid pointee ctype, but the pointee type has a size of zero\n", desc.m_pName);
            }
        }
        else if (has_pointee_ctype)
            vogl_warning_printf("ctype %s is not a pointer, but has a pointee ctype!\n", desc.m_pName);
        else if (desc.m_is_opaque_pointer)
            vogl_warning_printf("ctype %s is not a pointer, but has been marked as an opaque pointer!\n", desc.m_pName);
    }

    get_vogl_process_gl_ctypes().init();

    if (vogl::check_for_command_line_param("--vogl_debug"))
        vogl_dump_gl_ctypes();

    // sanity checks
    for (uint32_t ctype_iter = 0; ctype_iter < VOGL_NUM_CTYPES; ctype_iter++)
    {
        vogl_ctype_desc_t &desc = g_vogl_ctype_descs[ctype_iter];
        if ((desc.m_is_pointer_diff) ||
            (desc.m_loki_type_flags & LOKI_TYPE_BITMASK(LOKI_IS_POINTER)))
        {
            // This will fail on long long and unsigned long long, but GL/GLX don't use those types.
            VOGL_ASSERT(desc.m_size == sizeof(void *));
        }
        else
        {
            VOGL_ASSERT(!desc.m_is_pointer && !desc.m_is_opaque_pointer);
        }
    }
}
