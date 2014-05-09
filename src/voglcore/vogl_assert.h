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

// File: vogl_assert.h
#pragma once

#include "vogl_core.h"

const unsigned int VOGL_FAIL_EXCEPTION_CODE = 256U;
void vogl_enable_fail_exceptions(bool enabled);

// TODO: Add an optional message argument.
void vogl_assert(const char *pExp, const char *pFile, unsigned line) CLANG_ANALYZER_NORETURN;
void vogl_fail(const char *pExp, const char *pFile, unsigned line) CLANG_ANALYZER_NORETURN;

// Assertions are usually only enabled in debug builds.
#if defined(NDEBUG) && !VOGL_ENABLE_ASSERTIONS_IN_ALL_BUILDS
#define VOGL_ASSERT(x) ((void)0)
#undef VOGL_ASSERTS_ENABLED
#else
#define VOGL_ASSERT(_exp) (void)((!!(_exp)) || (vogl_assert(#_exp, __FILE__, __LINE__), 0))
#define VOGL_ASSERTS_ENABLED
#endif

// VOGL_ASSERT_ALWAYS is equivalent to VOGL_ASSERT(0) (not "always" does not imply release/optimized builds)
#define VOGL_ASSERT_ALWAYS VOGL_ASSERT(0)

// VOGL_VERIFY() is enabled in all builds.
#define VOGL_VERIFY(_exp) (void)((!!(_exp)) || (vogl_assert(#_exp, __FILE__, __LINE__), 0))

// VOGL_FAIL() is enabled in all builds and exits the process (unless vogl_enable_fail_exceptions(false) is called)
#define VOGL_FAIL(msg)                       \
    do                                         \
    {                                          \
        vogl_fail(#msg, __FILE__, __LINE__); \
    } while (0)

#define VOGL_ASSERT_OPEN_RANGE(x, l, h) VOGL_ASSERT((x >= l) && (x < h))
#define VOGL_ASSERT_CLOSED_RANGE(x, l, h) VOGL_ASSERT((x >= l) && (x <= h))

void trace(const char *pFmt, va_list args);
void trace(const char *pFmt, ...) VOGL_ATTRIBUTE_PRINTF(1, 2);

// Borrowed from boost libraries.
template <bool x>
struct vogl_assume_failure;
template <>
struct vogl_assume_failure<true>
{
    enum
    {
        blah = 1
    };
};
template <int x>
struct vogl_assume_try
{
};

#define VOGL_JOINER_FINAL(a, b) a##b
#define VOGL_JOINER(a, b) VOGL_JOINER_FINAL(a, b)
#define VOGL_JOIN(a, b) VOGL_JOINER(a, b)
#define VOGL_ASSUME(p) typedef vogl_assume_try<sizeof(vogl_assume_failure<(bool)(p)>)> VOGL_JOIN(vogl_assume_typedef, __COUNTER__)

#if defined(NDEBUG) && !VOGL_ENABLE_ASSERTIONS_IN_ALL_BUILDS
template <typename T>
inline T vogl_assert_range(T i, T m)
{
    VOGL_NOTE_UNUSED(m);
    return i;
}
template <typename T>
inline T vogl_assert_range_incl(T i, T m)
{
    VOGL_NOTE_UNUSED(m);
    return i;
}
#else
template <typename T>
inline T vogl_assert_range(T i, T m)
{
    VOGL_ASSERT((i >= 0) && (i < m));
    return i;
}

template <>
inline unsigned int vogl_assert_range<unsigned int>(unsigned int i, unsigned int m)
{
    VOGL_ASSERT(i < m);
    return i;
}

template <>
inline unsigned long long vogl_assert_range<unsigned long long>(unsigned long long i, unsigned long long m)
{
    VOGL_ASSERT(i < m);
    return i;
}

template <typename T>
inline T vogl_assert_range_incl(T i, T m)
{
    VOGL_ASSERT((i >= 0) && (i <= m));
    return i;
}

// These specializations are here to fix (useless) gcc compiler warnings
#if (COMPILER_GCC)
	template <>
	inline unsigned int vogl_assert_range_incl<uint32_t>(unsigned int i, unsigned int m)
	{
		VOGL_ASSERT(i <= m);
		return i;
	}
	template <>
	inline unsigned long long vogl_assert_range_incl<unsigned long long>(unsigned long long i, unsigned long long m)
	{
		VOGL_ASSERT(i <= m);
		return i;
	}
#endif
#endif
