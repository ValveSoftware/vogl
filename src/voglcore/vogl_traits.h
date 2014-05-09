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

// File: vogl_traits.h
#pragma once

#include "vogl_core.h"

namespace vogl
{
    template <typename T>
    struct int_traits
    {
        enum
        {
            cMin = vogl::cINT32_MIN,
            cMax = vogl::cINT32_MAX,
            cSigned = true
        };
    };

    template <>
    struct int_traits<int8_t>
    {
        enum
        {
            cMin = vogl::cINT8_MIN,
            cMax = vogl::cINT8_MAX,
            cSigned = true
        };
    };
    template <>
    struct int_traits<int16_t>
    {
        enum
        {
            cMin = vogl::cINT16_MIN,
            cMax = vogl::cINT16_MAX,
            cSigned = true
        };
    };
    template <>
    struct int_traits<int32_t>
    {
        enum
        {
            cMin = vogl::cINT32_MIN,
            cMax = vogl::cINT32_MAX,
            cSigned = true
        };
    };

    template <>
    struct int_traits<uint8_t>
    {
        enum
        {
            cMin = 0,
            cMax = vogl::cUINT8_MAX,
            cSigned = false
        };
    };
    template <>
    struct int_traits<uint16_t>
    {
        enum
        {
            cMin = 0,
            cMax = vogl::cUINT16_MAX,
            cSigned = false
        };
    };
    template <>
    struct int_traits<uint32_t>
    {
        enum
        {
            cMin = 0,
            cMax = vogl::cUINT32_MAX,
            cSigned = false
        };
    };

    // scalar_type<T>::construct, construct_array, etc. ensures integral types are initialized to 0 when constructed
    template <typename T>
    struct scalar_type
    {
        enum
        {
            cFlag = false
        };
        static inline void construct(T *p)
        {
            helpers::construct(p);
        }
        static inline void construct(T *p, const T &init)
        {
            helpers::construct(p, init);
        }
        static inline void construct_array(T *p, uint32_t n)
        {
            helpers::construct_array(p, n);
        }
        static inline void destruct(T *p)
        {
            helpers::destruct(p);
        }
        static inline void destruct_array(T *p, uint32_t n)
        {
            helpers::destruct_array(p, n);
        }
    };

    template <typename T>
    struct scalar_type<T *>
    {
        enum
        {
            cFlag = true
        };
        static inline void construct(T **p)
        {
            memset(p, 0, sizeof(T *));
        }
        static inline void construct(T **p, T *init)
        {
            *p = init;
        }
        static inline void construct_array(T **p, uint32_t n)
        {
            memset(p, 0, sizeof(T *) * n);
        }
        static inline void destruct(T **p)
        {
            VOGL_NOTE_UNUSED(p);
        }
        static inline void destruct_array(T **p, uint32_t n)
        {
            VOGL_NOTE_UNUSED(p), VOGL_NOTE_UNUSED(n);
        }
    };

#define VOGL_DEFINE_BUILT_IN_TYPE(X)                    \
    template <> struct scalar_type<X>                     \
    {                                                     \
        enum { cFlag = true };                            \
        static inline void construct(X *p)                \
        {                                                 \
            memset(p, 0, sizeof(X));                      \
        }                                                 \
        static inline void construct(X *p, const X &init) \
        {                                                 \
            memcpy(p, &init, sizeof(X));                  \
        }                                                 \
        static inline void construct_array(X *p, uint32_t n)  \
        {                                                 \
            memset(p, 0, sizeof(X) * n);                  \
        }                                                 \
        static inline void destruct(X *p)                 \
        {                                                 \
            VOGL_NOTE_UNUSED(p);                        \
        }                                                 \
        static inline void destruct_array(X *p, uint32_t n)   \
        {                                                 \
            VOGL_NOTE_UNUSED(p), VOGL_NOTE_UNUSED(n); \
        }                                                 \
    };

    VOGL_DEFINE_BUILT_IN_TYPE(bool)
    VOGL_DEFINE_BUILT_IN_TYPE(char)
    VOGL_DEFINE_BUILT_IN_TYPE(unsigned char)
    VOGL_DEFINE_BUILT_IN_TYPE(short)
    VOGL_DEFINE_BUILT_IN_TYPE(unsigned short)
    VOGL_DEFINE_BUILT_IN_TYPE(int)
    VOGL_DEFINE_BUILT_IN_TYPE(unsigned int)
    VOGL_DEFINE_BUILT_IN_TYPE(long)
    VOGL_DEFINE_BUILT_IN_TYPE(unsigned long)
#if defined(COMPILER_GCCLIKE)
    VOGL_DEFINE_BUILT_IN_TYPE(long long)
    VOGL_DEFINE_BUILT_IN_TYPE(unsigned long long)
#else
    VOGL_DEFINE_BUILT_IN_TYPE(__int64)
    VOGL_DEFINE_BUILT_IN_TYPE(unsigned __int64)
#endif
    VOGL_DEFINE_BUILT_IN_TYPE(float)
    VOGL_DEFINE_BUILT_IN_TYPE(double)
    VOGL_DEFINE_BUILT_IN_TYPE(long double)

#undef VOGL_DEFINE_BUILT_IN_TYPE

    // See: http://erdani.org/publications/cuj-2004-06.pdf

    template <typename T>
    struct bitwise_movable
    {
        enum
        {
            cFlag = false
        };
    };

// Defines type Q as bitwise movable. Use with types requiring destruction. DO NOT use with types that contain pointers into themselves.
// Bitwise movable: type T may be safely moved to a new location via memcpy, without requiring the old copy to be destructed.
// However, the final version of the object (wherever it winds up in memory) must be eventually destructed (a single time, of course).
// Bitwise movable is a superset of bitwise copyable (all bitwise copyable types are also bitwise movable).
#define VOGL_DEFINE_BITWISE_MOVABLE(Q)  \
    template <> struct bitwise_movable<Q> \
    {                                     \
        enum { cFlag = true };            \
    };

    template <typename T>
    struct bitwise_copyable
    {
        enum
        {
            cFlag = false
        };
    };

// Defines type Q as bitwise copyable. This is NOT SAFE for use with types that require destruction. DO NOT use with types that contain pointers into themselves.
// Bitwise copyable: type T may be safely and freely copied (duplicated) via memcpy, and *does not* require destruction.
#define VOGL_DEFINE_BITWISE_COPYABLE(Q)  \
    template <> struct bitwise_copyable<Q> \
    {                                      \
        enum { cFlag = true };             \
    };

#define VOGL_IS_POD(T) __is_pod(T)

#define VOGL_IS_SCALAR_TYPE(T) (scalar_type<T>::cFlag)

#define VOGL_IS_BITWISE_COPYABLE(T) (VOGL_IS_SCALAR_TYPE(T) || VOGL_IS_POD(T) || (bitwise_copyable<T>::cFlag))

#define VOGL_IS_BITWISE_COPYABLE_OR_MOVABLE(T) (VOGL_IS_BITWISE_COPYABLE(T) || (bitwise_movable<T>::cFlag))

#define VOGL_HAS_DESTRUCTOR(T) ((!scalar_type<T>::cFlag) && (!__is_pod(T)))

    // From yasli_traits.h:
    // Credit goes to Boost;
    // also found in the C++ Templates book by Vandevoorde and Josuttis

    typedef char (&yes_t)[1];
    typedef char (&no_t)[2];

    template <class U>
    yes_t class_test(int U::*);
    template <class U>
    no_t class_test(...);

    template <class T>
    struct is_class
    {
        enum
        {
            value = (sizeof(class_test<T>(0)) == sizeof(yes_t))
        };
    };

    template <typename T>
    struct is_pointer
    {
        enum
        {
            value = false
        };
    };

    template <typename T>
    struct is_pointer<T *>
    {
        enum
        {
            value = true
        };
    };

    VOGL_DEFINE_BITWISE_COPYABLE(empty_type);
    VOGL_DEFINE_BITWISE_MOVABLE(empty_type);

    namespace helpers
    {
        // dst must be uninitialized memory, src will be destructed
        template <typename T>
        inline void move(T &dst, T &src)
        {
            if (VOGL_IS_BITWISE_COPYABLE_OR_MOVABLE(T))
                memcpy(&dst, &src, sizeof(T));
            else
            {
                construct(&dst, src);
                destruct(&src);
            }
        }

        // pDst must be uninitialized memory, pSrc will be destructed
        template <typename T>
        inline void move_array(T *pDst, T *pSrc, uint32_t n)
        {
            if (VOGL_IS_BITWISE_COPYABLE_OR_MOVABLE(T))
                memcpy(pDst, pSrc, sizeof(T) * n);
            else
            {
                for (uint32_t i = 0; i < n; i++)
                {
                    construct(pDst + i, pSrc[i]);
                    destruct(pSrc + i);
                }
            }
        }

        // pDst must be uninitialized memory
        template <typename T>
        inline void copy_array(T *pDst, const T *pSrc, uint32_t n)
        {
            if (VOGL_IS_BITWISE_COPYABLE(T))
                memcpy(pDst, pSrc, sizeof(T) * n);
            else
            {
                for (uint32_t i = 0; i < n; i++)
                    construct(pDst + i, pSrc[i]);
            }
        }
    }

    // This doesn't invoke RTTI - but the return name is not portable so only use it for debugging purposes.
    // You can use vogl::demangle() to demangle the returned string.
    template <typename T>
    inline const char *type_name()
    {
        return typeid(T).name();
    }

    template <typename T>
    inline const char *type_name(const T &obj)
    {
        VOGL_NOTE_UNUSED(obj);
        return typeid(T).name();
    }

} // namespace vogl
