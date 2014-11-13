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

// File: vogl_build_options.h
#pragma once

// To try and separate concerns as much as possible, we use cmake to spit out
// friendly defines for us into several groups.
// For each group below, exactly one value will be set with a value of 1. The others will be undefined.

// [ COMPLIER_CLANG, COMPILER_GCC, COMPILER_MINGW, COMPILER_MSVC ] - The compiler being used.
// [ PLATFORM_32BIT, PLATFORM_64BIT ] -  Whether the target platform is 32 or 64 bits. 
// [ PLATFORM_LINUX, PLATFORM_OSX, PLATFORM_WINDOWS ] - OS of the target platform.

// There is also PLATFORM_POSIX, which indicates this is a POSIX environment (LINUX OR OSX is true).
// There is also COMPILER_GCCLIKE, which indicates that __GNUC__ is defined, which may mean CLANG, GCC or MINGW. 

#if !(defined(COMPILER_CLANG) || defined(COMPILER_GCC) || defined(COMPILER_MINGW) || defined(COMPILER_MSVC))
    #error "Compiler wasn't set, or a new compiler has been added!"
#endif

#if !(defined(PLATFORM_32BIT) || defined(PLATFORM_64BIT))
    #error "Platform bitcount wasn't set (or this code lived long enough for crazy platforms)!"
#endif

#if !(defined(PLATFORM_LINUX) || defined(PLATFORM_OSX) || defined(PLATFORM_WINDOWS))
    #error "Target platform wasn't set (or new target added)!"
#endif

#if defined(COMPILER_MSVC)
    #pragma warning(disable : 4201) // nonstandard extension used : nameless struct/union
    #pragma warning(disable : 4127) // conditional expression is constant
    #pragma warning(disable : 4793) // function compiled as native
    #pragma warning(disable : 4324) // structure was padded due to __declspec(align())
    #pragma warning(disable : 4100) // unreferenced formal parameter
#endif

#if defined(PLATFORM_WINDOWS) && !defined(VOGL_ANSI_CPLUSPLUS)
    #define VOGL_PLATFORM_PC 1
    #define VOGL_USE_WIN32_API 1

    // MSVC or MinGW, x86 or x64, Win32 API's for threading and Win32 Interlocked API's or GCC built-ins for atomic ops.
    #ifdef NDEBUG
        // Ensure checked iterators are disabled. Note: Be sure anything else that links against this lib also #define's this stuff, or remove this crap!
        #define _SECURE_SCL 0
        #define _HAS_ITERATOR_DEBUGGING 0
    #endif

    #ifndef _DLL
        // If we're using the DLL form of the run-time libs, we're also going to be enabling exceptions because we'll be building CLR apps.
        // Otherwise, we disable exceptions for a small speed boost.
        #define _HAS_EXCEPTIONS 0
    #endif

    #define NOMINMAX

    #if defined(COMPILER_MINGW)
        #define VOGL_USE_GCC_ATOMIC_BUILTINS 1
    #else
        #define VOGL_USE_WIN32_ATOMIC_FUNCTIONS 1
    #endif

    #if defined(PLATFORM_64BIT)
        #define VOGL_PLATFORM_PC_X64 1
        #define VOGL_64BIT_POINTERS 1
        #define VOGL_CPU_HAS_64BIT_REGISTERS 1
        #define VOGL_LITTLE_ENDIAN_CPU 1
    #else
        #define VOGL_PLATFORM_PC_X86 1
        #define VOGL_64BIT_POINTERS 0
        #define VOGL_CPU_HAS_64BIT_REGISTERS 0
        #define VOGL_LITTLE_ENDIAN_CPU 1
    #endif

    // We use the windows pthreads api.
    #define VOGL_USE_PTHREADS_API 1
    #define VOGL_NOTE_UNUSED(x) (void)(x)

    #define VOGL_USE_UNALIGNED_INT_LOADS 1
    #define VOGL_RESTRICT __restrict
    #define VOGL_FORCE_INLINE __forceinline
    #define VOGL_SELECT_ANY __declspec(selectany)

    #define VOGL_INIT_PRIORITY(prio) /* empty, temporarily */

    #if defined(COMPILER_MSVC) || defined(COMPILER_MINGW)
        #define VOGL_USE_MSVC_INTRINSICS 1
    #endif

    #define VOGL_STDCALL __stdcall
    #define VOGL_MEMORY_IMPORT_BARRIER
    #define VOGL_MEMORY_EXPORT_BARRIER

    // for gcc/clang: string_index is the param index of the format string, 1 is the first param for static funcs, 2 for member funcs
    // first_index is the first param to check, specify 0 to just check the format string for consistency
    #define VOGL_ATTRIBUTE_PRINTF(string_index, first_index)

#elif defined(COMPILER_GCCLIKE) && !defined(VOGL_ANSI_CPLUSPLUS)
    // GCC x86 or x64, pthreads for threading and GCC built-ins for atomic ops.
    #define VOGL_PLATFORM_PC 1
    #if defined(PLATFORM_LINUX)
        #define VOGL_USE_LINUX_API 1
    #elif defined(PLATFORM_OSX)
        #define VOGL_USE_OSX_API 1
    #endif

    #if defined(PLATFORM_64BIT) || defined(__MINGW64__) || defined(_LP64) || defined(__LP64__)
        #define VOGL_PLATFORM_PC_X64 1
        #define VOGL_64BIT_POINTERS 1
        #define VOGL_CPU_HAS_64BIT_REGISTERS 1
    #else
        #define VOGL_PLATFORM_PC_X86 1
        #define VOGL_64BIT_POINTERS 0
        #define VOGL_CPU_HAS_64BIT_REGISTERS 0
    #endif

    #define VOGL_USE_UNALIGNED_INT_LOADS 1

    #define VOGL_LITTLE_ENDIAN_CPU 1

    #define VOGL_USE_PTHREADS_API 1
    #define VOGL_USE_GCC_ATOMIC_BUILTINS 1

    #define VOGL_RESTRICT

    #if defined(COMPILER_CLANG)
        // FIXME - clang interprets forceinline in a subtly different way vs. gcc/MSVS, which causes multiple func definitions
        #define VOGL_FORCE_INLINE inline
    #else
        #define VOGL_FORCE_INLINE inline __attribute__((__always_inline__, __gnu_inline__))
    #endif

    #define VOGL_SELECT_ANY

    #define VOGL_INIT_PRIORITY(prio) __attribute__((init_priority(prio)))

    #define VOGL_STDCALL
    #define VOGL_MEMORY_IMPORT_BARRIER
    #define VOGL_MEMORY_EXPORT_BARRIER

    #define VOGL_ATTRIBUTE_PRINTF(string_index, first_index) __attribute__((format(printf, string_index, first_index)))

    #define VOGL_NOTE_UNUSED(x) (void) x

    #ifndef _LARGEFILE64_SOURCE
        #define _LARGEFILE64_SOURCE 1
    #endif

    #ifndef D_FILE_OFFSET_BITS
        #define _FILE_OFFSET_BITS 64
    #endif
#else
    // Vanilla ANSI-C/C++
    // No threading support, unaligned loads are NOT okay.
    #if defined(_WIN64) || defined(__MINGW64__) || defined(_LP64) || defined(__LP64__)
        #define VOGL_64BIT_POINTERS 1
        #define VOGL_CPU_HAS_64BIT_REGISTERS 1
    #else
        #define VOGL_64BIT_POINTERS 0
        #define VOGL_CPU_HAS_64BIT_REGISTERS 0
    #endif

    #define VOGL_USE_UNALIGNED_INT_LOADS 0

    #if __BIG_ENDIAN__
        #define VOGL_BIG_ENDIAN_CPU 1
    #else
        #define VOGL_LITTLE_ENDIAN_CPU 1
    #endif

    #define VOGL_USE_GCC_ATOMIC_BUILTINS 0
    #define VOGL_USE_WIN32_ATOMIC_FUNCTIONS 0

    #define VOGL_RESTRICT
    #define VOGL_FORCE_INLINE inline
    #define VOGL_SELECT_ANY

    #define VOGL_STDCALL
    #define VOGL_MEMORY_IMPORT_BARRIER
    #define VOGL_MEMORY_EXPORT_BARRIER

    #define VOGL_ATTRIBUTE_PRINTF(string_index, first_index)

    #define VOGL_NOTE_UNUSED(x) (void) x
#endif

#ifndef VOGL_ENABLE_ASSERTIONS_IN_ALL_BUILDS
    #define VOGL_ENABLE_ASSERTIONS_IN_ALL_BUILDS 0
#endif

#if PLATFORM_WINDOWS
    #define _CRT_RAND_S 1
#endif

#if defined(PLATFORM_LINUX)
    #define VOGL_HAS_PROC_FILESYSTEM 1
    #define VOGL_PLATFORM_HAS_GLX 1
    #define VOGL_PLATFORM_HAS_CGL 0
    #define VOGL_PLATFORM_HAS_WGL 0
    #define VOGL_PLATFORM_SUPPORTS_BTRACE 1
    #define VOGL_PLATFORM_HAS_X11 1
    #define VOGL_PLATFORM_HAS_UINT 1
    #define VOGL_PLATFORM_HAS_SDL 1

#elif defined(PLATFORM_OSX)
    #define VOGL_HAS_PROC_FILESYSTEM 0
    #define VOGL_PLATFORM_HAS_GLX 0
    #define VOGL_PLATFORM_HAS_CGL 1
    #define VOGL_PLATFORM_HAS_WGL 0
    #define VOGL_PLATFORM_SUPPORTS_BTRACE 0
    #define VOGL_PLATFORM_HAS_X11 0
    #define VOGL_PLATFORM_HAS_UINT 1
    #define VOGL_PLATFORM_HAS_SDL 1

#elif defined(PLATFORM_WINDOWS)
    #define VOGL_HAS_PROC_FILESYSTEM 0
    #define VOGL_PLATFORM_HAS_GLX 0
    #define VOGL_PLATFORM_HAS_CGL 0
    #define VOGL_PLATFORM_HAS_WGL 1
    #define VOGL_PLATFORM_SUPPORTS_BTRACE 0
    #define VOGL_PLATFORM_HAS_X11 0
    #define VOGL_PLATFORM_HAS_UINT 0
    #define VOGL_PLATFORM_HAS_SDL 1

#else
    #error "Specify whether target platform has PROC filesystem or not."
#endif

