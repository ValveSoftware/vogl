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

// File: vogl_helpers.h
#pragma once

#include "vogl_core.h"

#define VOGL_NO_COPY_OR_ASSIGNMENT_OP(c) \
    c(const c &);                          \
    c &operator=(const c &);
#define VOGL_NO_HEAP_ALLOC()         \
    \
private:                               \
    static void *operator new(size_t); \
    static void *operator new[](size_t);

template <typename DestType, typename SrcType>
VOGL_FORCE_INLINE DestType safe_int_cast(SrcType src_val)
{
	DestType ret_val = DestType(src_val);
	VOGL_ASSERT(ret_val == src_val);

	return ret_val;
}

namespace vogl
{
    namespace helpers
    {
        template <typename T>
        struct rel_ops
        {
            friend bool operator!=(const T &x, const T &y)
            {
                return (!(x == y));
            }
            friend bool operator>(const T &x, const T &y)
            {
                return (y < x);
            }
            friend bool operator<=(const T &x, const T &y)
            {
                return (!(y < x));
            }
            friend bool operator>=(const T &x, const T &y)
            {
                return (!(x < y));
            }
        };

        template <typename T>
        inline T *construct(T *p)
        {
            return new (static_cast<void *>(p)) T;
        }

        template <typename T, typename U>
        inline T *construct(T *p, const U &init)
        {
            return new (static_cast<void *>(p)) T(init);
        }

        template <typename T>
        inline void construct_array(T *p, uint32_t n)
        {
            T *q = p + n;
            for (; p != q; ++p)
                new (static_cast<void *>(p)) T;
        }

        template <typename T, typename U>
        inline void construct_array(T *p, uint32_t n, const U &init)
        {
            T *q = p + n;
            for (; p != q; ++p)
                new (static_cast<void *>(p)) T(init);
        }

        template <typename T>
        inline void destruct(T *p)
        {
            p->~T();
        }

        template <typename T>
        inline void destruct_array(T *p, uint32_t n)
        {
            T *q = p + n;
            for (; p != q; ++p)
                p->~T();
        }

    } // namespace helpers

} // namespace vogl
