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

// File: vogl_core.h
#ifndef VOGL_CORE_H
#define VOGL_CORE_H
#pragma once

#include <inttypes.h>

#if defined(WIN32) && defined(_MSC_VER)
#pragma warning(disable : 4201) // nonstandard extension used : nameless struct/union
#pragma warning(disable : 4127) // conditional expression is constant
#pragma warning(disable : 4793) // function compiled as native
#pragma warning(disable : 4324) // structure was padded due to __declspec(align())
#pragma warning(disable : 4100) // unreferenced formal parameter
#endif

#if defined(WIN32) && !defined(VOGL_ANSI_CPLUSPLUS)
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

#if defined(__MINGW32__) || defined(__MINGW64__)
#define VOGL_USE_GCC_ATOMIC_BUILTINS 1
#else
#define VOGL_USE_WIN32_ATOMIC_FUNCTIONS 1
#endif

#if defined(_WIN64) || defined(__MINGW64__) || defined(_LP64) || defined(__LP64__)
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

#define VOGL_USE_UNALIGNED_INT_LOADS 1
#define VOGL_RESTRICT __restrict
#define VOGL_FORCE_INLINE __forceinline
#define VOGL_SELECT_ANY __declspec(selectany)

#if defined(_MSC_VER) || defined(__MINGW32__) || defined(__MINGW64__)
#define VOGL_USE_MSVC_INTRINSICS 1
#endif

#define VOGL_STDCALL __stdcall
#define VOGL_MEMORY_IMPORT_BARRIER
#define VOGL_MEMORY_EXPORT_BARRIER

#define VOGL_FUNCTION_NAME __FUNCTION__

// for gcc/clang: string_index is the param index of the format string, 1 is the first param for static funcs, 2 for member funcs
// first_index is the first param to check, specify 0 to just check the format string for consistency
#define VOGL_ATTRIBUTE_PRINTF(string_index, first_index)

#define VOGL_NOTE_UNUSED(x) (void)(x)

#elif defined(__GNUC__) && !defined(VOGL_ANSI_CPLUSPLUS)
// GCC x86 or x64, pthreads for threading and GCC built-ins for atomic ops.
#define VOGL_PLATFORM_PC 1
#ifdef __linux
#define VOGL_USE_LINUX_API 1
#endif

#if defined(_WIN64) || defined(__MINGW64__) || defined(_LP64) || defined(__LP64__)
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

#ifdef __clang__
// FIXME - clang interprets forceinline in a subtly different way vs. gcc/MSVS, which causes multiple func definitions
#define VOGL_FORCE_INLINE inline
#else
#define VOGL_FORCE_INLINE inline __attribute__((__always_inline__, __gnu_inline__))
#endif

#define VOGL_SELECT_ANY

#define VOGL_STDCALL
#define VOGL_MEMORY_IMPORT_BARRIER
#define VOGL_MEMORY_EXPORT_BARRIER

#define VOGL_FUNCTION_NAME __FUNCTION__

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

#define VOGL_FUNCTION_NAME __FUNCTION__

#define VOGL_ATTRIBUTE_PRINTF(string_index, first_index)

#define VOGL_NOTE_UNUSED(x) (void) x
#endif

#ifndef VOGL_ENABLE_ASSERTIONS_IN_ALL_BUILDS
#define VOGL_ENABLE_ASSERTIONS_IN_ALL_BUILDS 0
#endif

#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
#include <math.h>
#include <stdarg.h>
#include <string.h>
#include <memory.h>
#include <limits.h>
#include <errno.h>
#include <stdint.h>

#include <locale>
#include <algorithm>
#include <limits>
#include <typeinfo>
#include <functional>
#include <iterator>

#ifdef min
#undef min
#endif

#ifdef max
#undef max
#endif

#define VOGL_FALSE (0)
#define VOGL_TRUE (1)
#define VOGL_MAX_PATH (260)

#define VOGL_NAMESPACE_BEGIN(x) \
    namespace x                   \
    {
#define VOGL_NAMESPACE_END(x) }

#if defined(_DEBUG) || defined(DEBUG)
#define VOGL_BUILD_DEBUG

#if defined(NDEBUG)
#error NDEBUG cannot be defined in VOGL_BUILD_DEBUG
#endif
#else
#define VOGL_BUILD_RELEASE

#ifndef NDEBUG
#define NDEBUG
#endif

#if defined(DEBUG) || defined(_DEBUG)
#error DEBUG and _DEBUG cannot be defined in VOGL_BUILD_RELEASE
#endif
#endif

#include "vogl_warnings.h"
#include "vogl_types.h"
#include "vogl_assert.h"
#include "vogl_platform.h"
#include "vogl_helpers.h"
#include "vogl_traits.h"
#include "vogl_mem.h"
#include "vogl_math.h"
#include "vogl_utils.h"
#include "vogl_hash.h"
#include "vogl_rand.h"
#include "vogl_introsort.h"
#include "vogl_vector.h"
#include "vogl_fixed_string.h"
#include "vogl_dynamic_string.h"
#include "vogl_timer.h"

#endif // VOGL_CORE_H
