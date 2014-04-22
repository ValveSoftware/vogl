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

#if defined(__GNUC__)
  #define GCC_DIAGNOSTIC_STR(s)       #s
  #define GCC_DIAGNOSTIC_JOINSTR(x,y) GCC_DIAGNOSTIC_STR(x ## y)
  #define GCC_DIAGNOSTIC_DO_PRAGMA(x) _Pragma (#x)
  #define GCC_DIAGNOSTIC_PRAGMA(x)    GCC_DIAGNOSTIC_DO_PRAGMA(GCC diagnostic x)

  #define GCC_DIAGNOSTIC_PUSH()       GCC_DIAGNOSTIC_PRAGMA(push)
  #define GCC_DIAGNOSTIC_IGNORED(x)   GCC_DIAGNOSTIC_PRAGMA(ignored GCC_DIAGNOSTIC_JOINSTR(-W,x))
  #define GCC_DIAGNOSTIC_POP()        GCC_DIAGNOSTIC_PRAGMA(pop)
#else
  #define GCC_DIAGNOSTIC_PUSH()
  #define GCC_DIAGNOSTIC_IGNORED(x)
  #define GCC_DIAGNOSTIC_POP()
#endif

#if !defined(__clang__) && !defined(__has_feature)
  #define __has_feature(_x) 0
#endif

#if !defined(CLANG_ANALYZER_NORETURN)
  #if __has_feature(attribute_analyzer_noreturn)
    #define CLANG_ANALYZER_NORETURN __attribute__((analyzer_noreturn))
  #else
    #define CLANG_ANALYZER_NORETURN
  #endif
#endif

#ifdef _MSC_VER
#define VOGL_NORETURN __declspec(noreturn)
#elif defined(__GNUC__)
#define VOGL_NORETURN __attribute__((noreturn))
#else
#define VOGL_NORETURN
#endif
