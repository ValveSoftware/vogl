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

/* utility definitions */
#ifdef _POSIX2_RE_DUP_MAX
#define DUPMAX _POSIX2_RE_DUP_MAX
#else
#define DUPMAX 255
#endif
#define INFINITY (DUPMAX + 1)
#define NC (CHAR_MAX - CHAR_MIN + 1)
typedef unsigned char uch;

/* switch off assertions (if not already off) if no REDEBUG */
#ifndef REDEBUG
#ifndef NDEBUG
#define NDEBUG /* no assertions please */
#endif
#endif
#include <assert.h>

/* for old systems with bcopy() but no memmove() */
#ifdef USEBCOPY
#define memmove(d, s, c) bcopy(s, d, c)
#endif
