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

// File: vogl_utils.h
#pragma once

#if defined(VOGL_USE_LINUX_API)
#include <time.h>
#endif
#include "vogl_core.h"

#define VOGL_OFFSETOF(t, e) ((uint32)(intptr_t)(&((t *)(0))->e))

#define VOGL_MIN(a, b) (((a) < (b)) ? (a) : (b))
#define VOGL_MAX(a, b) (((a) < (b)) ? (b) : (a))

#define VOGL_GLUER(a, b) a##b

#ifdef COMPILER_MSVC
// Need to explictly extern these with MSVC, but not MinGW.
extern "C" unsigned long __cdecl _lrotl(unsigned long, int);
#pragma intrinsic(_lrotl)

extern "C" unsigned long __cdecl _lrotr(unsigned long, int);
#pragma intrinsic(_lrotr)
#endif

#ifdef PLATFORM_WINDOWS
#define VOGL_ROTATE_LEFT(x, k) _lrotl(x, k)
#define VOGL_ROTATE_RIGHT(x, k) _lrotr(x, k)
#else
#define VOGL_ROTATE_LEFT(x, k) (((x) << (k)) | ((x) >> (32 - (k))))
#define VOGL_ROTATE_RIGHT(x, k) (((x) >> (k)) | ((x) << (32 - (k))))
#endif

template <class T, size_t N>
T decay_array_to_subtype(T (&a)[N]);
#define VOGL_ARRAY_SIZE(X) (sizeof(X) / sizeof(decay_array_to_subtype(X)))

#define VOGL_SIZEOF_U32(x) static_cast<uint32>(sizeof(x))

// Returns # of bits needed to encode x, i.e. 0=0, 1=1, 0xFFFF=15, 0x10000=16, etc.
#define VOGL_SIGNIFICANT_BITS(x)                                                               \
    (!!((x) & 0x80000000) + !!((x) & 0xc0000000) + !!((x) & 0xe0000000) + !!((x) & 0xf0000000) + \
     !!((x) & 0xf8000000) + !!((x) & 0xfc000000) + !!((x) & 0xfe000000) + !!((x) & 0xff000000) + \
     !!((x) & 0xff800000) + !!((x) & 0xffc00000) + !!((x) & 0xffe00000) + !!((x) & 0xfff00000) + \
     !!((x) & 0xfff80000) + !!((x) & 0xfffc0000) + !!((x) & 0xfffe0000) + !!((x) & 0xffff0000) + \
     !!((x) & 0xffff8000) + !!((x) & 0xffffc000) + !!((x) & 0xffffe000) + !!((x) & 0xfffff000) + \
     !!((x) & 0xfffff800) + !!((x) & 0xfffffc00) + !!((x) & 0xfffffe00) + !!((x) & 0xffffff00) + \
     !!((x) & 0xffffff80) + !!((x) & 0xffffffc0) + !!((x) & 0xffffffe0) + !!((x) & 0xfffffff0) + \
     !!((x) & 0xfffffff8) + !!((x) & 0xfffffffc) + !!((x) & 0xfffffffe) + !!((x) & 0xffffffff))

#define VOGL_HAS_NULLBYTE(x) (((x) - 0x01010101) & ~(x) & 0x80808080)

namespace vogl
{
    namespace utils
    {
        template <typename T>
        struct relative_ops
        {
            friend bool operator>(const T &a, const T &b)
            {
                return b < a;
            }
            friend bool operator<=(const T &a, const T &b)
            {
                return !(b < a);
            }
            friend bool operator>=(const T &a, const T &b)
            {
                return !(a < b);
            }
            friend bool operator!=(const T &a, const T &b)
            {
                return !(a == b);
            }
        };

        // Given a less than comparison functor comp, compare a and b
        template <typename T, typename F>
        bool compare_lt(const T &a, const T &b, F comp)
        {
            return comp(a, b);
        }
        template <typename T, typename F>
        bool compare_le(const T &a, const T &b, F comp)
        {
            return !comp(b, a);
        }
        template <typename T, typename F>
        bool compare_gt(const T &a, const T &b, F comp)
        {
            return comp(b, a);
        }
        template <typename T, typename F>
        bool compare_ge(const T &a, const T &b, F comp)
        {
            return !comp(a, b);
        }
        template <typename T, typename F>
        bool compare_eq(const T &a, const T &b, F comp)
        {
            return !comp(a, b) && !comp(b, a);
        }
        template <typename T, typename F>
        bool compare_neq(const T &a, const T &b, F comp)
        {
            return comp(a, b) || comp(b, a);
        }

        template <typename T>
        inline void swap(T &l, T &r)
        {
            // Using std::swap() here now because it has C++0x optimizations and it can swap C-style arrays.
            std::swap(l, r);
        }

        template <typename T>
        inline void zero_object(T &obj)
        {
            memset((void *)&obj, 0, sizeof(obj));
        }

        template <typename T>
        inline void zero_this(T *pObj)
        {
            memset((void *)pObj, 0, sizeof(*pObj));
        }

        inline bool is_bit_set(uint bits, uint mask)
        {
            return (bits & mask) != 0;
        }

        inline void set_bit(uint &bits, uint mask, bool state)
        {
            if (state)
                bits |= mask;
            else
                bits &= ~mask;
        }

        inline bool is_flag_set(uint bits, uint flag)
        {
            VOGL_ASSERT(flag < 32U);
            return is_bit_set(bits, 1U << flag);
        }

        inline void set_flag(uint &bits, uint flag, bool state)
        {
            VOGL_ASSERT(flag < 32U);
            set_bit(bits, 1U << flag, state);
        }

        inline void invert_buf(void *pBuf, uint size)
        {
            uint8 *p = static_cast<uint8 *>(pBuf);

            const uint half_size = size >> 1;
            for (uint i = 0; i < half_size; i++)
                utils::swap(p[i], p[size - 1U - i]);
        }

        // buffer_is_little_endian is the endianness of the buffer's data
        template <typename T>
        inline void write_obj(const T &obj, void *pBuf, bool buffer_is_little_endian)
        {
            const uint8 *pSrc = reinterpret_cast<const uint8 *>(&obj);
            uint8 *pDst = static_cast<uint8 *>(pBuf);

            if (c_vogl_little_endian_platform == buffer_is_little_endian)
                memcpy(pDst, pSrc, sizeof(T));
            else
            {
                for (uint i = 0; i < sizeof(T); i++)
                    pDst[i] = pSrc[sizeof(T) - 1 - i];
            }
        }

        // buffer_is_little_endian is the endianness of the buffer's data
        template <typename T>
        inline void read_obj(T &obj, const void *pBuf, bool buffer_is_little_endian)
        {
            const uint8 *pSrc = reinterpret_cast<const uint8 *>(pBuf);
            uint8 *pDst = reinterpret_cast<uint8 *>(&obj);

            if (c_vogl_little_endian_platform == buffer_is_little_endian)
                memcpy(pDst, pSrc, sizeof(T));
            else
            {
                for (uint i = 0; i < sizeof(T); i++)
                    pDst[i] = pSrc[sizeof(T) - 1 - i];
            }
        }

        template <typename T>
        inline bool write_obj(const T &obj, void *&pBuf, uint &buf_size, bool buffer_is_little_endian)
        {
            if (buf_size < sizeof(T))
                return false;

            utils::write_obj(obj, pBuf, buffer_is_little_endian);

            pBuf = static_cast<uint8 *>(pBuf) + sizeof(T);
            buf_size -= sizeof(T);

            return true;
        }

        inline bool write_val(uint8 val, void *&pBuf, uint &buf_size, bool buffer_is_little_endian)
        {
            return write_obj(val, pBuf, buf_size, buffer_is_little_endian);
        }
        inline bool write_val(uint16 val, void *&pBuf, uint &buf_size, bool buffer_is_little_endian)
        {
            return write_obj(val, pBuf, buf_size, buffer_is_little_endian);
        }
        inline bool write_val(uint val, void *&pBuf, uint &buf_size, bool buffer_is_little_endian)
        {
            return write_obj(val, pBuf, buf_size, buffer_is_little_endian);
        }
        inline bool write_val(int val, void *&pBuf, uint &buf_size, bool buffer_is_little_endian)
        {
            return write_obj(val, pBuf, buf_size, buffer_is_little_endian);
        }
        inline bool write_val(uint64_t val, void *&pBuf, uint &buf_size, bool buffer_is_little_endian)
        {
            return write_obj(val, pBuf, buf_size, buffer_is_little_endian);
        }
        inline bool write_val(float val, void *&pBuf, uint &buf_size, bool buffer_is_little_endian)
        {
            return write_obj(val, pBuf, buf_size, buffer_is_little_endian);
        }
        inline bool write_val(double val, void *&pBuf, uint &buf_size, bool buffer_is_little_endian)
        {
            return write_obj(val, pBuf, buf_size, buffer_is_little_endian);
        }

        template <typename T>
        inline bool read_obj(T &obj, const void *&pBuf, uint &buf_size, bool buffer_is_little_endian)
        {
            if (buf_size < sizeof(T))
            {
                zero_object(obj);
                return false;
            }

            utils::read_obj(obj, pBuf, buffer_is_little_endian);

            pBuf = static_cast<const uint8 *>(pBuf) + sizeof(T);
            buf_size -= sizeof(T);

            return true;
        }

#if defined(COMPILER_MSVC)
        VOGL_FORCE_INLINE uint16 swap16(uint16 x)
        {
            return _byteswap_ushort(x);
        }
        VOGL_FORCE_INLINE uint32 swap32(uint32 x)
        {
            return _byteswap_ulong(x);
        }
        VOGL_FORCE_INLINE uint64_t swap64(uint64_t x)
        {
            return _byteswap_uint64(x);
        }
#elif defined(COMPILER_GCCLIKE)
        VOGL_FORCE_INLINE uint16 swap16(uint16 x)
        {
            return static_cast<uint16>((x << 8U) | (x >> 8U));
        }
        VOGL_FORCE_INLINE uint32 swap32(uint32 x)
        {
            return __builtin_bswap32(x);
        }
        VOGL_FORCE_INLINE uint64_t swap64(uint64_t x)
        {
            return __builtin_bswap64(x);
        }
#else
        VOGL_FORCE_INLINE uint16 swap16(uint16 x)
        {
            return static_cast<uint16>((x << 8U) | (x >> 8U));
        }
        VOGL_FORCE_INLINE uint32 swap32(uint32 x)
        {
            return ((x << 24U) | ((x << 8U) & 0x00FF0000U) | ((x >> 8U) & 0x0000FF00U) | (x >> 24U));
        }
        VOGL_FORCE_INLINE uint64_t swap64(uint64_t x)
        {
            return (static_cast<uint64_t>(swap32(static_cast<uint32>(x))) << 32ULL) | swap32(static_cast<uint32>(x >> 32U));
        }
#endif

        // Assumes x has been read from memory as a little endian value, converts to native endianness for manipulation.
        VOGL_FORCE_INLINE uint16 swap_le16_to_native(uint16 x)
        {
            return c_vogl_little_endian_platform ? x : swap16(x);
        }
        VOGL_FORCE_INLINE uint32 swap_le32_to_native(uint32 x)
        {
            return c_vogl_little_endian_platform ? x : swap32(x);
        }
        VOGL_FORCE_INLINE uint64_t swap_le64_to_native(uint64_t x)
        {
            return c_vogl_little_endian_platform ? x : swap64(x);
        }

        // Assumes x has been read from memory as a big endian value, converts to native endianness for manipulation.
        VOGL_FORCE_INLINE uint16 swap_be16_to_native(uint16 x)
        {
            return c_vogl_big_endian_platform ? x : swap16(x);
        }
        VOGL_FORCE_INLINE uint32 swap_be32_to_native(uint32 x)
        {
            return c_vogl_big_endian_platform ? x : swap32(x);
        }
        VOGL_FORCE_INLINE uint64_t swap_be64_to_native(uint64_t x)
        {
            return c_vogl_big_endian_platform ? x : swap64(x);
        }

        VOGL_FORCE_INLINE uint32 read_le32(const void *p)
        {
            return swap_le32_to_native(*static_cast<const uint32 *>(p));
        }
        VOGL_FORCE_INLINE void write_le32(void *p, uint32 x)
        {
            *static_cast<uint32 *>(p) = swap_le32_to_native(x);
        }
        VOGL_FORCE_INLINE uint64_t read_le64(const void *p)
        {
            return swap_le64_to_native(*static_cast<const uint64_t *>(p));
        }
        VOGL_FORCE_INLINE void write_le64(void *p, uint64_t x)
        {
            *static_cast<uint64_t *>(p) = swap_le64_to_native(x);
        }

        VOGL_FORCE_INLINE uint32 read_be32(const void *p)
        {
            return swap_be32_to_native(*static_cast<const uint32 *>(p));
        }
        VOGL_FORCE_INLINE void write_be32(void *p, uint32 x)
        {
            *static_cast<uint32 *>(p) = swap_be32_to_native(x);
        }
        VOGL_FORCE_INLINE uint64_t read_be64(const void *p)
        {
            return swap_be64_to_native(*static_cast<const uint64_t *>(p));
        }
        VOGL_FORCE_INLINE void write_be64(void *p, uint64_t x)
        {
            *static_cast<uint64_t *>(p) = swap_be64_to_native(x);
        }

        inline void endian_swap_mem16(uint16 *p, uint n)
        {
            while (n--)
            {
                *p = swap16(*p);
                ++p;
            }
        }
        inline void endian_swap_mem32(uint32 *p, uint n)
        {
            while (n--)
            {
                *p = swap32(*p);
                ++p;
            }
        }
        inline void endian_swap_mem64(uint64_t *p, uint n)
        {
            while (n--)
            {
                *p = swap64(*p);
                ++p;
            }
        }

        inline void endian_swap_mem(void *p, uint size_in_bytes, uint type_size)
        {
            switch (type_size)
            {
                case sizeof(uint16)
                    :
                    endian_swap_mem16(static_cast<uint16 *>(p), size_in_bytes / type_size);
                    break;
                case sizeof(uint32)
                    :
                    endian_swap_mem32(static_cast<uint32 *>(p), size_in_bytes / type_size);
                    break;
                case sizeof(uint64_t)
                    :
                    endian_swap_mem64(static_cast<uint64_t *>(p), size_in_bytes / type_size);
                    break;
            }
        }

        inline void fast_memset(void *pDst, int val, size_t size)
        {
            memset(pDst, val, size);
        }

        inline void fast_memcpy(void *pDst, const void *pSrc, size_t size)
        {
            memcpy(pDst, pSrc, size);
        }

        inline uint count_leading_zeros(uint v)
        {
            uint temp;
            uint n = 32;

            temp = v >> 16;
            if (temp)
            {
                n -= 16;
                v = temp;
            }

            temp = v >> 8;
            if (temp)
            {
                n -= 8;
                v = temp;
            }

            temp = v >> 4;
            if (temp)
            {
                n -= 4;
                v = temp;
            }

            temp = v >> 2;
            if (temp)
            {
                n -= 2;
                v = temp;
            }

            temp = v >> 1;
            if (temp)
            {
                n -= 1;
                v = temp;
            }

            if (v & 1)
                n--;

            return n;
        }

        inline uint count_leading_zeros16(uint v)
        {
            VOGL_ASSERT(v < 0x10000);

            uint temp;
            uint n = 16;

            temp = v >> 8;
            if (temp)
            {
                n -= 8;
                v = temp;
            }

            temp = v >> 4;
            if (temp)
            {
                n -= 4;
                v = temp;
            }

            temp = v >> 2;
            if (temp)
            {
                n -= 2;
                v = temp;
            }

            temp = v >> 1;
            if (temp)
            {
                n -= 1;
                v = temp;
            }

            if (v & 1)
                n--;

            return n;
        }

        void endian_switch_words(uint16 *p, uint num);
        void endian_switch_dwords(uint32 *p, uint num);
        void copy_words(uint16 *pDst, const uint16 *pSrc, uint num, bool endian_switch);
        void copy_dwords(uint32 *pDst, const uint32 *pSrc, uint num, bool endian_switch);

        // Returns the maximum number of mip levels given the specified width/height, including the first (largest) mip level.
        uint compute_max_mips(uint width, uint height, uint min_width = 1U, uint min_height = 1U);
        uint compute_max_mips3D(uint width, uint height, uint depth, uint min_width = 1U, uint min_height = 1U, uint min_depth = 1U);

        inline char to_hex(uint v)
        {
            VOGL_ASSERT(v <= 0xF);
            if (v < 10)
                return '0' + v;
            else if (v <= 0xF)
                return 'A' + v - 10;
            return 0;
        }

        inline int from_hex(char c)
        {
            if ((c >= '0') && (c <= '9'))
                return c - '0';
            else if ((c >= 'a') && (c <= 'f'))
                return (c - 'a') + 10;
            else if ((c >= 'A') && (c <= 'F'))
                return (c - 'A') + 10;
            return -1;
        }

        bool is_buffer_printable(const void *pBuf, uint buf_size, bool include_crlf, bool expect_null_terminator);

        extern const int64_t s_signed_mins[9];
        extern const int64_t s_signed_maxs[9];
        extern const uint64_t s_unsigned_maxes[9];

        inline int64_t get_signed_min_value(uint integral_type_size)
        {
            VOGL_ASSERT(integral_type_size <= 8);
            int64_t val = (integral_type_size <= 8) ? s_signed_mins[integral_type_size] : 0;
            VOGL_ASSERT((!integral_type_size) || (val == (-s_signed_maxs[integral_type_size] - 1)));
            return val;
        }

        inline int64_t get_signed_max_value(uint integral_type_size)
        {
            VOGL_ASSERT(integral_type_size <= 8);
            int64_t val = (integral_type_size <= 8) ? s_signed_maxs[integral_type_size] : 0;
            VOGL_ASSERT((!integral_type_size) || (val == -(s_signed_mins[integral_type_size] + 1)));
            return val;
        }

        inline uint64_t get_unsigned_max_value(uint integral_type_size)
        {
            VOGL_ASSERT(integral_type_size <= 8);
            uint64_t val = (integral_type_size <= 8) ? s_unsigned_maxes[integral_type_size] : 0;
            VOGL_ASSERT((integral_type_size == 8) ? (val == cUINT64_MAX) : (val == ((1ULL << (integral_type_size * 8U)) - 1ULL)));
            uint64_t k = (static_cast<uint64_t>(1ULL | (s_signed_maxs[integral_type_size]) << 1U));
            VOGL_NOTE_UNUSED(k);
            VOGL_ASSERT((!integral_type_size) || (val == k));
            return val;
        }

        inline bool is_8bit(uint64_t val)
        {
            return val <= cUINT8_MAX;
        }

        inline bool is_8bit(int64_t val)
        {
            return (val >= cINT8_MIN) && (val <= cINT8_MAX);
        }

        inline bool is_16bit(uint64_t val)
        {
            return val <= cUINT16_MAX;
        }

        inline bool is_16bit(int64_t val)
        {
            return (val >= cINT16_MIN) && (val <= cINT16_MAX);
        }

        inline bool is_32bit(uint64_t val)
        {
            return val <= cUINT32_MAX;
        }

        inline bool is_32bit(int64_t val)
        {
            return (val >= cINT32_MIN) && (val <= cINT32_MAX);
        }

        // map_value: attempts to map the specified value from F to T, or returns def_value if the map isn't possible

        template <typename V, typename F, typename T>
        inline T map_value(V value, T def_value, F f0, T t0)
        {
            if (value == f0)
                return t0;
            return def_value;
        }

        template <typename V, typename F, typename T>
        inline T map_value(V value, T def_value, F f0, T t0, F f1, T t1)
        {
            if (value == f0)
                return t0;
            else if (value == f1)
                return t1;
            return def_value;
        }

        template <typename V, typename F, typename T>
        inline T map_value(V value, T def_value, F f0, T t0, F f1, T t1, F f2, T t2)
        {
            if (value == f0)
                return t0;
            else if (value == f1)
                return t1;
            else if (value == f2)
                return t2;
            return def_value;
        }

        template <typename V, typename F, typename T>
        inline T map_value(V value, T def_value, F f0, T t0, F f1, T t1, F f2, T t2, F f3, T t3)
        {
            if (value == f0)
                return t0;
            else if (value == f1)
                return t1;
            else if (value == f2)
                return t2;
            else if (value == f3)
                return t3;
            return def_value;
        }

        template <typename V, typename F, typename T>
        inline T map_value(V value, T def_value, F f0, T t0, F f1, T t1, F f2, T t2, F f3, T t3, F f4, T t4)
        {
            if (value == f0)
                return t0;
            else if (value == f1)
                return t1;
            else if (value == f2)
                return t2;
            else if (value == f3)
                return t3;
            else if (value == f4)
                return t4;
            return def_value;
        }

        template <typename V, typename F, typename T>
        inline T map_value(V value, T def_value, F f0, T t0, F f1, T t1, F f2, T t2, F f3, T t3, F f4, T t4, F f5, T t5)
        {
            if (value == f0)
                return t0;
            else if (value == f1)
                return t1;
            else if (value == f2)
                return t2;
            else if (value == f3)
                return t3;
            else if (value == f4)
                return t4;
            else if (value == f5)
                return t5;
            return def_value;
        }

        template <typename V, typename F, typename T>
        inline T map_value(V value, T def_value, F f0, T t0, F f1, T t1, F f2, T t2, F f3, T t3, F f4, T t4, F f5, T t5, F f6, T t6)
        {
            if (value == f0)
                return t0;
            else if (value == f1)
                return t1;
            else if (value == f2)
                return t2;
            else if (value == f3)
                return t3;
            else if (value == f4)
                return t4;
            else if (value == f5)
                return t5;
            else if (value == f6)
                return t6;
            return def_value;
        }

        template <typename V, typename F, typename T>
        inline T map_value(V value, T def_value, F f0, T t0, F f1, T t1, F f2, T t2, F f3, T t3, F f4, T t4, F f5, T t5, F f6, T t6, F f7, T t7)
        {
            if (value == f0)
                return t0;
            else if (value == f1)
                return t1;
            else if (value == f2)
                return t2;
            else if (value == f3)
                return t3;
            else if (value == f4)
                return t4;
            else if (value == f5)
                return t5;
            else if (value == f6)
                return t6;
            else if (value == f7)
                return t7;
            return def_value;
        }

        template <typename V, typename F, typename T>
        inline T map_value(V value, T def_value, F f0, T t0, F f1, T t1, F f2, T t2, F f3, T t3, F f4, T t4, F f5, T t5, F f6, T t6, F f7, T t7, F f8, T t8)
        {
            if (value == f0)
                return t0;
            else if (value == f1)
                return t1;
            else if (value == f2)
                return t2;
            else if (value == f3)
                return t3;
            else if (value == f4)
                return t4;
            else if (value == f5)
                return t5;
            else if (value == f6)
                return t6;
            else if (value == f7)
                return t7;
            else if (value == f8)
                return t8;
            return def_value;
        }

        template <typename V, typename F, typename T>
        inline T map_value(V value, T def_value, F f0, T t0, F f1, T t1, F f2, T t2, F f3, T t3, F f4, T t4, F f5, T t5, F f6, T t6, F f7, T t7, F f8, T t8, F f9, T t9)
        {
            if (value == f0)
                return t0;
            else if (value == f1)
                return t1;
            else if (value == f2)
                return t2;
            else if (value == f3)
                return t3;
            else if (value == f4)
                return t4;
            else if (value == f5)
                return t5;
            else if (value == f6)
                return t6;
            else if (value == f7)
                return t7;
            else if (value == f8)
                return t8;
            else if (value == f9)
                return t9;
            return def_value;
        }

        template <typename V, typename F, typename T>
        inline T map_value(V value, T def_value, F f0, T t0, F f1, T t1, F f2, T t2, F f3, T t3, F f4, T t4, F f5, T t5, F f6, T t6, F f7, T t7, F f8, T t8, F f9, T t9, F f10, T t10)
        {
            if (value == f0)
                return t0;
            else if (value == f1)
                return t1;
            else if (value == f2)
                return t2;
            else if (value == f3)
                return t3;
            else if (value == f4)
                return t4;
            else if (value == f5)
                return t5;
            else if (value == f6)
                return t6;
            else if (value == f7)
                return t7;
            else if (value == f8)
                return t8;
            else if (value == f9)
                return t9;
            else if (value == f10)
                return t10;
            return def_value;
        }

        // is_in_set: true if value is either v0, v1, etc.

        template <typename V, typename T>
        inline bool is_in_set(V value, T v0)
        {
            return value == v0;
        }

        template <typename V, typename T>
        inline bool is_in_set(V value, T v0, T v1)
        {
            return (value == v0) || (value == v1);
        }

        template <typename V, typename T>
        inline bool is_in_set(V value, T v0, T v1, T v2)
        {
            return (value == v0) || (value == v1) || (value == v2);
        }

        template <typename V, typename T>
        inline bool is_in_set(V value, T v0, T v1, T v2, T v3)
        {
            return (value == v0) || (value == v1) || (value == v2) || (value == v3);
        }

        template <typename V, typename T>
        inline bool is_in_set(V value, T v0, T v1, T v2, T v3, T v4)
        {
            return (value == v0) || (value == v1) || (value == v2) || (value == v3) || (value == v4);
        }

        template <typename V, typename T>
        inline bool is_in_set(V value, T v0, T v1, T v2, T v3, T v4, T v5)
        {
            return (value == v0) || (value == v1) || (value == v2) || (value == v3) || (value == v4) || (value == v5);
        }

        template <typename V, typename T>
        inline bool is_in_set(V value, T v0, T v1, T v2, T v3, T v4, T v5, T v6)
        {
            return (value == v0) || (value == v1) || (value == v2) || (value == v3) || (value == v4) || (value == v5) || (value == v6);
        }

        template <typename V, typename T>
        inline bool is_in_set(V value, T v0, T v1, T v2, T v3, T v4, T v5, T v6, T v7)
        {
            return (value == v0) || (value == v1) || (value == v2) || (value == v3) || (value == v4) || (value == v5) || (value == v6) || (value == v7);
        }

        template <typename V, typename T>
        inline bool is_in_set(V value, T v0, T v1, T v2, T v3, T v4, T v5, T v6, T v7, T v8)
        {
            return (value == v0) || (value == v1) || (value == v2) || (value == v3) || (value == v4) || (value == v5) || (value == v6) || (value == v7) || (value == v8);
        }

        template <typename V, typename T>
        inline bool is_in_set(V value, T v0, T v1, T v2, T v3, T v4, T v5, T v6, T v7, T v8, T v9)
        {
            return (value == v0) || (value == v1) || (value == v2) || (value == v3) || (value == v4) || (value == v5) || (value == v6) || (value == v7) || (value == v8) || (value == v9);
        }

        template <typename V, typename T>
        inline bool is_in_set(V value, T v0, T v1, T v2, T v3, T v4, T v5, T v6, T v7, T v8, T v9, T v10)
        {
            return (value == v0) || (value == v1) || (value == v2) || (value == v3) || (value == v4) || (value == v5) || (value == v6) || (value == v7) || (value == v8) || (value == v9) || (value == v10);
        }

        template <typename V, typename T>
        inline bool is_in_set(V value, T v0, T v1, T v2, T v3, T v4, T v5, T v6, T v7, T v8, T v9, T v10, T v11)
        {
            return (value == v0) || (value == v1) || (value == v2) || (value == v3) || (value == v4) || (value == v5) || (value == v6) || (value == v7) || (value == v8) || (value == v9) || (value == v10) || (value == v11);
        }

        // is_not_in_set: true if value is != v0, and != v1, etc.

        template <typename V, typename T>
        inline bool is_not_in_set(V value, T v0)
        {
            return value != v0;
        }

        template <typename V, typename T>
        inline bool is_not_in_set(V value, T v0, T v1)
        {
            return (value != v0) && (value != v1);
        }

        template <typename V, typename T>
        inline bool is_not_in_set(V value, T v0, T v1, T v2)
        {
            return (value != v0) && (value != v1) && (value != v2);
        }

        template <typename V, typename T>
        inline bool is_not_in_set(V value, T v0, T v1, T v2, T v3)
        {
            return (value != v0) && (value != v1) && (value != v2) && (value != v3);
        }

        template <typename V, typename T>
        inline bool is_not_in_set(V value, T v0, T v1, T v2, T v3, T v4)
        {
            return (value != v0) && (value != v1) && (value != v2) && (value != v3) && (value != v4);
        }

        template <typename V, typename T>
        inline bool is_not_in_set(V value, T v0, T v1, T v2, T v3, T v4, T v5)
        {
            return (value != v0) && (value != v1) && (value != v2) && (value != v3) && (value != v4) && (value != v5);
        }

        template <typename V, typename T>
        inline bool is_not_in_set(V value, T v0, T v1, T v2, T v3, T v4, T v5, T v6)
        {
            return (value != v0) && (value != v1) && (value != v2) && (value != v3) && (value != v4) && (value != v5) && (value != v6);
        }

        template <typename V, typename T>
        inline bool is_not_in_set(V value, T v0, T v1, T v2, T v3, T v4, T v5, T v6, T v7)
        {
            return (value != v0) && (value != v1) && (value != v2) && (value != v3) && (value != v4) && (value != v5) && (value != v6) && (value != v7);
        }

        template <typename V, typename T>
        inline bool is_not_in_set(V value, T v0, T v1, T v2, T v3, T v4, T v5, T v6, T v7, T v8)
        {
            return (value != v0) && (value != v1) && (value != v2) && (value != v3) && (value != v4) && (value != v5) && (value != v6) && (value != v7) && (value != v8);
        }

        // map_to_index: tries to map value v0, v1, and returns its index, or returns def

        template <typename V, typename T>
        inline int map_to_index(V value, int def, T v0)
        {
            if (value == v0)
                return 0;
            return def;
        }

        template <typename V, typename T>
        inline int map_to_index(V value, int def, T v0, T v1)
        {
            if (value == v0)
                return 0;
            else if (value == v1)
                return 1;
            return def;
        }

        template <typename V, typename T>
        inline int map_to_index(V value, int def, T v0, T v1, T v2)
        {
            if (value == v0)
                return 0;
            else if (value == v1)
                return 1;
            else if (value == v2)
                return 2;
            return def;
        }

        template <typename V, typename T>
        inline int map_to_index(V value, int def, T v0, T v1, T v2, T v3)
        {
            if (value == v0)
                return 0;
            else if (value == v1)
                return 1;
            else if (value == v2)
                return 2;
            else if (value == v3)
                return 3;
            return def;
        }

        template <typename V, typename T>
        inline int map_to_index(V value, int def, T v0, T v1, T v2, T v3, T v4)
        {
            if (value == v0)
                return 0;
            else if (value == v1)
                return 1;
            else if (value == v2)
                return 2;
            else if (value == v3)
                return 3;
            else if (value == v4)
                return 4;
            return def;
        }

        template <typename V, typename T>
        inline int map_to_index(V value, int def, T v0, T v1, T v2, T v3, T v4, T v5)
        {
            if (value == v0)
                return 0;
            else if (value == v1)
                return 1;
            else if (value == v2)
                return 2;
            else if (value == v3)
                return 3;
            else if (value == v4)
                return 4;
            else if (value == v5)
                return 5;
            return def;
        }

        template <typename V, typename T>
        inline int map_to_index(V value, int def, T v0, T v1, T v2, T v3, T v4, T v5, T v6)
        {
            if (value == v0)
                return 0;
            else if (value == v1)
                return 1;
            else if (value == v2)
                return 2;
            else if (value == v3)
                return 3;
            else if (value == v4)
                return 4;
            else if (value == v5)
                return 5;
            else if (value == v6)
                return 6;
            return def;
        }

        template <typename V, typename T>
        inline int map_to_index(V value, int def, T v0, T v1, T v2, T v3, T v4, T v5, T v6, T v7)
        {
            if (value == v0)
                return 0;
            else if (value == v1)
                return 1;
            else if (value == v2)
                return 2;
            else if (value == v3)
                return 3;
            else if (value == v4)
                return 4;
            else if (value == v5)
                return 5;
            else if (value == v6)
                return 6;
            else if (value == v7)
                return 7;
            return def;
        }

        template <typename V, typename T>
        inline int map_to_index(V value, int def, T v0, T v1, T v2, T v3, T v4, T v5, T v6, T v7, T v8)
        {
            if (value == v0)
                return 0;
            else if (value == v1)
                return 1;
            else if (value == v2)
                return 2;
            else if (value == v3)
                return 3;
            else if (value == v4)
                return 4;
            else if (value == v5)
                return 5;
            else if (value == v6)
                return 6;
            else if (value == v7)
                return 7;
            else if (value == v8)
                return 8;
            return def;
        }

        // Finds value in the specified static array, or -1 if it can't be found
        template <typename V, typename T, size_t size>
        inline int64_t find_value_in_array(V value, T (&p)[size])
        {
            for (size_t i = 0; i < size; i++)
                if (value == p[i])
                    return i;
            return -1;
        }

        // Finds value in the specified array, or -1 if it can't be found
        template <typename V, typename T>
        inline int64_t find_value_in_array(V value, T *p, size_t size)
        {
            for (size_t i = 0; i < size; i++)
                if (value == p[i])
                    return i;
            return -1;
        }

        // Should be called before using RDTSC().
        bool init_rdtsc();

        inline uint64_t get_rdtsc()
        {
#if defined(COMPILER_GCCLIKE)
            unsigned int hi, lo;
            __asm__ volatile("rdtsc" : "=a"(lo), "=d"(hi));
            return ((uint64_t)hi << 32) | lo;
#else
            return __rdtsc();
#endif
        }

        inline uint64_t RDTSC()
        {
#if defined(VOGL_USE_LINUX_API)
            extern int g_reliable_rdtsc;
            if (g_reliable_rdtsc == -1)
                init_rdtsc();
            if (g_reliable_rdtsc == 0)
            {
                //$ TODO: Should just use SDL_GetPerformanceCounter?
                struct timespec time;
                clock_gettime(CLOCK_MONOTONIC, &time);
                return ((uint64_t)time.tv_sec * 1000000000) + time.tv_nsec;
            }
#endif
            return get_rdtsc();
        }

    } // namespace utils

} // namespace vogl
