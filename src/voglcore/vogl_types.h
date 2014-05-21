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

// File: vogl_types.h
#pragma once

#include "vogl_core.h"

#if (!VOGL_PLATFORM_HAS_UINT)
    // Provide uint for other platforms for portability. 
    typedef unsigned int uint;
#endif

namespace vogl
{
    const uint8_t cUINT8_MIN = 0;
    const uint8_t cUINT8_MAX = 0xFFU;
    const uint16_t cUINT16_MIN = 0;
    const uint16_t cUINT16_MAX = 0xFFFFU;
    const uint32_t cUINT32_MIN = 0;
    const uint32_t cUINT32_MAX = 0xFFFFFFFFU;
    const uint64_t cUINT64_MIN = 0;
    const uint64_t cUINT64_MAX = 0xFFFFFFFFFFFFFFFFULL; //0xFFFFFFFFFFFFFFFFui64;

    const int8_t cINT8_MIN = -128;
    const int8_t cINT8_MAX = 127;
    const int16_t cINT16_MIN = -32768;
    const int16_t cINT16_MAX = 32767;
    const int32_t cINT32_MIN = (-2147483647 - 1);
    const int32_t cINT32_MAX = 2147483647;
    const int64_t cINT64_MIN = (int64_t)0x8000000000000000ULL; //(-9223372036854775807i64 - 1);
    const int64_t cINT64_MAX = (int64_t)0x7FFFFFFFFFFFFFFFULL; // 9223372036854775807i64;

    enum eVarArg
    {
        cVarArg
    };
    enum eClear
    {
        cClear
    };
    enum eZero
    {
        cZero
    };
    enum eNoClamp
    {
        cNoClamp
    };
    enum
    {
        cInvalidIndex = -1
    };
    enum eBitwise
    {
        cBitwise
    };

    const uint32_t cIntBits = 32;

    template <bool condition, class Then, class Else>
    struct template_if
    {
        typedef Then result;
    };
    template <class Then, class Else>
    struct template_if<false, Then, Else>
    {
        typedef Else result;
    };

    struct empty_type
    {
        inline bool operator==(const empty_type &rhs) const
        {
            VOGL_NOTE_UNUSED(rhs);
            return true;
        }
        inline bool operator<(const empty_type &rhs) const
        {
            VOGL_NOTE_UNUSED(rhs);
            return false;
        }
    };

    class json_value;

    inline bool json_serialize(const empty_type &, json_value &)
    {
        return false;
    }

    inline bool json_deserialize(empty_type &, const json_value &)
    {
        return false;
    }

    template <typename T>
    struct is_empty_type
    {
        enum
        {
            cValue = false
        };
    };

    template <>
    struct is_empty_type<empty_type>
    {
        enum
        {
            cValue = true
        };
    };

    uint32_t fast_hash(const void *p, int len);

    // Bitwise hasher: Directly hashes key's bits
    template <typename T>
    struct bit_hasher
    {
        inline size_t operator()(const T &key) const
        {
            return static_cast<size_t>(fast_hash(&key, sizeof(key)));
        }
    };

    // Intrusive hasher: Requires object implement a get_hash() method
    template <typename T>
    struct intrusive_hasher
    {
        inline size_t operator()(const T &key) const
        {
            return static_cast<size_t>(key.get_hash());
        }
    };

    // Default hasher: Casts object to size_t
    template <typename T>
    struct hasher
    {
        inline size_t operator()(const T &key) const
        {
            return static_cast<size_t>(key);
        }
    };

    // Now define some common specializations of hasher<> because it's the default used by hash_map and some types can't be static_cast to size_t.
    template <typename T>
    struct hasher<T *> : bit_hasher<T *>
    {
    };

#define VOGL_DEFINE_BITWISE_HASHER_SPECIALIZATION(T) \
    template <> struct hasher<T> : bit_hasher<T>       \
    {                                                  \
    }
    VOGL_DEFINE_BITWISE_HASHER_SPECIALIZATION(float);
    VOGL_DEFINE_BITWISE_HASHER_SPECIALIZATION(double);
    VOGL_DEFINE_BITWISE_HASHER_SPECIALIZATION(long double);
    VOGL_DEFINE_BITWISE_HASHER_SPECIALIZATION(int64_t);
    VOGL_DEFINE_BITWISE_HASHER_SPECIALIZATION(uint64_t);
#undef VOGL_DEFINE_BITWISE_HASHER_SPECIALIZATION

    template <typename T>
    struct equal_to
    {
        inline bool operator()(const T &a, const T &b) const
        {
            return a == b;
        }
    };

    template <typename T>
    struct equal_to_using_less_than
    {
        inline bool operator()(const T &a, const T &b) const
        {
            return !(a < b) && !(b < a);
        }
    };

    template <typename T>
    struct less_than
    {
        inline bool operator()(const T &a, const T &b) const
        {
            return a < b;
        }
    };
} // namespace vogl


