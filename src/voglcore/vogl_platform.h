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

// File: vogl_platform.h
#pragma once

#include "vogl_core.h"

bool vogl_is_debugger_present(void);
void vogl_debug_break_if_debugging(void);
void vogl_output_debug_string(const char *p);

// actually in vogl_assert.cpp
void vogl_assert(const char *pExp, const char *pFile, unsigned line);
void vogl_fail(const char *pExp, const char *pFile, unsigned line);

#if VOGL_LITTLE_ENDIAN_CPU
const bool c_vogl_little_endian_platform = true;
#else
const bool c_vogl_little_endian_platform = false;
#endif

const bool c_vogl_big_endian_platform = !c_vogl_little_endian_platform;

#if defined(VOGL_USE_LINUX_API)
    #define vogl_fopen(f, m) fopen64(f, m)
    #define vogl_fopen_s(pDstFile, f, m) *(pDstFile) = fopen64(f, m)
    #define vogl_fseek fseeko64
    #define vogl_ftell ftello64
#elif defined(VOGL_USE_OSX_API)
    #define vogl_fopen(f, m) fopen(f, m)
    #define vogl_fopen_s(pDstFile, f, m) *(pDstFile) = fopen(f, m)
    #define vogl_fseek fseeko
    #define vogl_ftell ftello
#elif defined(COMPILER_MSVC)
    inline FILE *vogl_fopen(const char *pFilename, const char *pMode)
    {
        FILE *pFile = NULL;
        fopen_s(&pFile, pFilename, pMode);
        return pFile;
    }
    #define vogl_fopen_s(pDstFile, f, m) fopen_s(pDstFile, f, m)
    #define vogl_fseek _fseeki64
    #define vogl_ftell _ftelli64
#else
    #define vogl_fopen(f, m) fopen(f, m)
    #define vogl_fopen_s(pDstFile, f, m) *(pDstFile) = fopen(f, m)
    #define vogl_fseek(s, o, w) fseek(s, static_cast<long>(o), w)
    #define vogl_ftell ftell
#endif

#define vogl_fread fread
#define vogl_fwrite fwrite
#define vogl_fputc fputc
#define vogl_fgetc fgetc
#define vogl_ungetc ungetc
#define vogl_fputs fputs
#define vogl_fgets fgets
#define vogl_fprintf fprintf
#define vogl_feof feof
#define vogl_fflush fflush
#define vogl_fclose fclose

#ifdef VOGL_USE_WIN32_API
    #define VOGL_BREAKPOINT DebugBreak();
    #define VOGL_BUILTIN_EXPECT(c, v) c
#elif defined(COMPILER_GCCLIKE)
    #define VOGL_BREAKPOINT asm("int $3");
    #define VOGL_BUILTIN_EXPECT(c, v) __builtin_expect(c, v)
#else
    #define VOGL_BREAKPOINT
    #define VOGL_BUILTIN_EXPECT(c, v) c
#endif

#define VOGL_CONDITIONAL_BREAKPOINT(_cond) do { if (!!(_cond)) { VOGL_BREAKPOINT; } } while(0)

#if defined(COMPILER_GCCLIKE)
    #define VOGL_ALIGNED(x) __attribute__((aligned(x)))
    #define VOGL_ALIGNED_BEGIN(x)
    #define VOGL_ALIGNED_END(x) __attribute__((aligned(x)))
    #define VOGL_NOINLINE __attribute__((noinline))
#elif defined(COMPILER_MSVC)
    #define VOGL_ALIGNED(x) __declspec(align(x))
    #define VOGL_ALIGNED_BEGIN(x) __declspec(align(x))
    #define VOGL_ALIGNED_END(x)
    #define VOGL_NOINLINE __declspec(noinline)
#else
    #define VOGL_ALIGNED(x)
    #define VOGL_NOINLINE
#endif

#define VOGL_GET_ALIGNMENT(v) ((!sizeof(v)) ? 1 : (__alignof(v) ? __alignof(v) : sizeof(uint32_t)))

#if defined(COMPILER_GCCLIKE)

    #define VOGL_CONSTRUCTOR_FUNCTION(func) \
        static void __attribute__((constructor)) func(void)

#elif defined(COMPILER_MSVC)

    // CRT Initialization. See:
    //  http://msdn.microsoft.com/en-us/library/bb918180.aspx
    #define VOGL_CONSTRUCTOR_FUNCTION(func) \
        static void __cdecl func(void); \
        __pragma(section(".CRT$XCU",read)) \
        __declspec(allocate(".CRT$XCU")) static void (__cdecl * func ## _entry)(void) = func;

#else
    #error "No VOGL_CONSTRUCTOR_FUNCTION definition."
#endif

inline bool vogl_is_little_endian()
{
    return c_vogl_little_endian_platform;
}
inline bool vogl_is_big_endian()
{
    return c_vogl_big_endian_platform;
}

inline bool vogl_is_pc()
{
#ifdef VOGL_PLATFORM_PC
    return true;
#else
    return false;
#endif
}

inline bool vogl_is_x86()
{
#ifdef VOGL_PLATFORM_PC_X86
    return true;
#else
    return false;
#endif
}

inline bool vogl_is_x64()
{
#ifdef VOGL_PLATFORM_PC_X64
    return true;
#else
    return false;
#endif
}

inline bool vogl_is_debug_build()
{
#ifdef VOGL_BUILD_DEBUG
    return true;
#else
    return false;
#endif
}

typedef void (*vogl_exception_callback_t)();
vogl_exception_callback_t vogl_set_exception_callback(vogl_exception_callback_t callback);
void vogl_reset_exception_callback();

