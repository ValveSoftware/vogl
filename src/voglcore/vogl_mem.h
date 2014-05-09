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

// File: vogl_mem.h
#pragma once

#include "vogl_core.h"

// Set to 1 to enable allocation debugging using rmalloc.c (much slower)
#ifndef VOGL_MALLOC_DEBUGGING
#define VOGL_MALLOC_DEBUGGING 0
#endif

#if defined(COMPILER_GCCLIKE)
#	define VOGL_NORETURN __attribute__((noreturn))
#elif defined(COMPILER_MSVC)
#	define VOGL_NORETURN __declspec(noreturn)
#else
#	error("Need VOGL_NORETURN definition for this compiler")
#endif

#ifndef VOGL_MIN_ALLOC_ALIGNMENT
// malloc() is 8 bytes for glibc in 32-bit, and 16 in 64-bit
// must be at least sizeof(uint32_t) * 2
#define VOGL_MIN_ALLOC_ALIGNMENT sizeof(void *) * 2
#endif

#define VOGL_FUNCTIONIZE(a, b) a(b)
#define VOGL_STRINGIZE(a) #a
#define VOGL_INT2STRING(i) VOGL_FUNCTIONIZE(VOGL_STRINGIZE, i)
#define VOGL_FILE_POS_STRING __FILE__ ":" VOGL_INT2STRING(__LINE__)

#if VOGL_MALLOC_DEBUGGING
#define VOGL_MALLOC_FILE_LEN_STRING VOGL_FILE_POS_STRING
#else
#define VOGL_MALLOC_FILE_LEN_STRING NULL
#endif

extern "C" void *vogl_realloc(const char *pFile_line, void *p, size_t new_size);

namespace vogl
{
#if VOGL_64BIT_POINTERS
    const uint64_t VOGL_MAX_POSSIBLE_HEAP_BLOCK_SIZE = 0x1000000000ULL; // 64GB should be big enough for anybody!
#else
    const uint32_t VOGL_MAX_POSSIBLE_HEAP_BLOCK_SIZE = 0x7FFF0000U; // ~1.999GB
#endif
    void vogl_init_heap();

    void *vogl_tracked_malloc(const char *pFile_line, size_t size, size_t *pActual_size = NULL);
    void *vogl_tracked_realloc(const char *pFile_line, void *p, size_t size, size_t *pActual_size = NULL);
    void *vogl_tracked_calloc(const char *pFile_line, size_t count, size_t size, size_t *pActual_size = NULL);
    void vogl_tracked_free(const char *pFile_line, void *p);
    void vogl_tracked_check_heap(const char *pFile_line);
    void vogl_tracked_print_stats(const char *pFile_line);

// C-style malloc/free/etc.
#define vogl_malloc(...) vogl::vogl_tracked_malloc(VOGL_MALLOC_FILE_LEN_STRING, __VA_ARGS__)
#define vogl_realloc(...) vogl::vogl_tracked_realloc(VOGL_MALLOC_FILE_LEN_STRING, __VA_ARGS__)
#define vogl_calloc(...) vogl::vogl_tracked_calloc(VOGL_MALLOC_FILE_LEN_STRING, __VA_ARGS__)
#define vogl_free(...) vogl::vogl_tracked_free(VOGL_MALLOC_FILE_LEN_STRING, __VA_ARGS__)
#define vogl_check_heap() vogl::vogl_tracked_check_heap(VOGL_FILE_POS_STRING)
#define vogl_print_heap_stats() vogl::vogl_tracked_print_stats(VOGL_FILE_POS_STRING)

    size_t vogl_msize(void *p);

	VOGL_NORETURN void vogl_mem_error(const char *pMsg, const char *pFile_line);

    // C++ new/delete wrappers that automatically pass in the file/line
    template <typename T>
    inline T *vogl_tracked_new(const char *pFile_line)
    {
        T *p = static_cast<T *>(vogl_tracked_malloc(pFile_line, sizeof(T)));
        if (VOGL_IS_SCALAR_TYPE(T))
            return p;
        return helpers::construct(p);
    }

    template <typename T, typename A>
    inline T *vogl_tracked_new(const char *pFile_line, const A &init0)
    {
        T *p = static_cast<T *>(vogl_tracked_malloc(pFile_line, sizeof(T)));
        return new (static_cast<void *>(p)) T(init0);
    }

    template <typename T, typename A>
    inline T *vogl_tracked_new(const char *pFile_line, A &init0)
    {
        T *p = static_cast<T *>(vogl_tracked_malloc(pFile_line, sizeof(T)));
        return new (static_cast<void *>(p)) T(init0);
    }

    template <typename T, typename A, typename B>
    inline T *vogl_tracked_new(const char *pFile_line, const A &init0, const B &init1)
    {
        T *p = static_cast<T *>(vogl_tracked_malloc(pFile_line, sizeof(T)));
        return new (static_cast<void *>(p)) T(init0, init1);
    }

    template <typename T, typename A, typename B, typename C>
    inline T *vogl_tracked_new(const char *pFile_line, const A &init0, const B &init1, const C &init2)
    {
        T *p = static_cast<T *>(vogl_tracked_malloc(pFile_line, sizeof(T)));
        return new (static_cast<void *>(p)) T(init0, init1, init2);
    }

    template <typename T, typename A, typename B, typename C, typename D>
    inline T *vogl_tracked_new(const char *pFile_line, const A &init0, const B &init1, const C &init2, const D &init3)
    {
        T *p = static_cast<T *>(vogl_tracked_malloc(pFile_line, sizeof(T)));
        return new (static_cast<void *>(p)) T(init0, init1, init2, init3);
    }

    template <typename T, typename A, typename B, typename C, typename D, typename E>
    inline T *vogl_tracked_new(const char *pFile_line, const A &init0, const B &init1, const C &init2, const D &init3, const E &init4)
    {
        T *p = static_cast<T *>(vogl_tracked_malloc(pFile_line, sizeof(T)));
        return new (static_cast<void *>(p)) T(init0, init1, init2, init3, init4);
    }

    template <typename T, typename A, typename B, typename C, typename D, typename E, typename F>
    inline T *vogl_tracked_new(const char *pFile_line, const A &init0, const B &init1, const C &init2, const D &init3, const E &init4, const F &init5)
    {
        T *p = static_cast<T *>(vogl_tracked_malloc(pFile_line, sizeof(T)));
        return new (static_cast<void *>(p)) T(init0, init1, init2, init3, init4, init5);
    }

    template <typename T, typename A, typename B, typename C, typename D, typename E, typename F, typename G>
    inline T *vogl_tracked_new(const char *pFile_line, const A &init0, const B &init1, const C &init2, const D &init3, const E &init4, const F &init5, const G &init6)
    {
        T *p = static_cast<T *>(vogl_tracked_malloc(pFile_line, sizeof(T)));
        return new (static_cast<void *>(p)) T(init0, init1, init2, init3, init4, init5, init6);
    }

    template <typename T, typename A, typename B, typename C, typename D, typename E, typename F, typename G, typename H>
    inline T *vogl_tracked_new(const char *pFile_line, const A &init0, const B &init1, const C &init2, const D &init3, const E &init4, const F &init5, const G &init6, const H &init7)
    {
        T *p = static_cast<T *>(vogl_tracked_malloc(pFile_line, sizeof(T)));
        return new (static_cast<void *>(p)) T(init0, init1, init2, init3, init4, init5, init6, init7);
    }

    template <typename T, typename A, typename B, typename C, typename D, typename E, typename F, typename G, typename H, typename I>
    inline T *vogl_tracked_new(const char *pFile_line, const A &init0, const B &init1, const C &init2, const D &init3, const E &init4, const F &init5, const G &init6, const H &init7, const I &init8)
    {
        T *p = static_cast<T *>(vogl_tracked_malloc(pFile_line, sizeof(T)));
        return new (static_cast<void *>(p)) T(init0, init1, init2, init3, init4, init5, init6, init7, init8);
    }

    template <typename T, typename A, typename B, typename C, typename D, typename E, typename F, typename G, typename H, typename I, typename J>
    inline T *vogl_tracked_new(const char *pFile_line, const A &init0, const B &init1, const C &init2, const D &init3, const E &init4, const F &init5, const G &init6, const H &init7, const I &init8, const J &init9)
    {
        T *p = static_cast<T *>(vogl_tracked_malloc(pFile_line, sizeof(T)));
        return new (static_cast<void *>(p)) T(init0, init1, init2, init3, init4, init5, init6, init7, init8, init9);
    }

    template <typename T, typename A, typename B, typename C, typename D, typename E, typename F, typename G, typename H, typename I, typename J, typename K>
    inline T *vogl_tracked_new(const char *pFile_line, const A &init0, const B &init1, const C &init2, const D &init3, const E &init4, const F &init5, const G &init6, const H &init7, const I &init8, const J &init9, const K &init10)
    {
        T *p = static_cast<T *>(vogl_tracked_malloc(pFile_line, sizeof(T)));
        return new (static_cast<void *>(p)) T(init0, init1, init2, init3, init4, init5, init6, init7, init8, init9, init10);
    }

    template <typename T, typename A, typename B, typename C, typename D, typename E, typename F, typename G, typename H, typename I, typename J, typename K, typename L>
    inline T *vogl_tracked_new(const char *pFile_line, const A &init0, const B &init1, const C &init2, const D &init3, const E &init4, const F &init5, const G &init6, const H &init7, const I &init8, const J &init9, const K &init10, const L &init11)
    {
        T *p = static_cast<T *>(vogl_tracked_malloc(pFile_line, sizeof(T)));
        return new (static_cast<void *>(p)) T(init0, init1, init2, init3, init4, init5, init6, init7, init8, init9, init10, init11);
    }

    template <typename T, typename A, typename B, typename C, typename D, typename E, typename F, typename G, typename H, typename I, typename J, typename K, typename L, typename M>
    inline T *vogl_tracked_new(const char *pFile_line, const A &init0, const B &init1, const C &init2, const D &init3, const E &init4, const F &init5, const G &init6, const H &init7, const I &init8, const J &init9, const K &init10, const L &init11, const M &init12)
    {
        T *p = static_cast<T *>(vogl_tracked_malloc(pFile_line, sizeof(T)));
        return new (static_cast<void *>(p)) T(init0, init1, init2, init3, init4, init5, init6, init7, init8, init9, init10, init11, init12);
    }

    template <typename T, typename A, typename B, typename C, typename D, typename E, typename F, typename G, typename H, typename I, typename J, typename K, typename L, typename M, typename N>
    inline T *vogl_tracked_new(const char *pFile_line, const A &init0, const B &init1, const C &init2, const D &init3, const E &init4, const F &init5, const G &init6, const H &init7, const I &init8, const J &init9, const K &init10, const L &init11, const M &init12, const N &init13)
    {
        T *p = static_cast<T *>(vogl_tracked_malloc(pFile_line, sizeof(T)));
        return new (static_cast<void *>(p)) T(init0, init1, init2, init3, init4, init5, init6, init7, init8, init9, init10, init11, init12, init13);
    }

    template <typename T>
    inline void vogl_tracked_delete(const char *pFile_line, T *p)
    {
        if (p)
        {
            if (!VOGL_IS_SCALAR_TYPE(T))
            {
                helpers::destruct(p);
            }
            vogl_tracked_free(pFile_line, p);
        }
    }

    // num is (obviously) limited to cUINT32_MAX, but the total allocated size in bytes can be > than this on x64
    template <typename T>
    inline T *vogl_tracked_new_array(const char *pFile_line, uint32_t num)
    {
        if (!num)
        {
            VOGL_ASSERT_ALWAYS;
            num = 1;
        }

        uint64_t total = VOGL_MIN_ALLOC_ALIGNMENT + static_cast<uint64_t>(sizeof(T)) * num;
        if ((total != static_cast<size_t>(total)) || (total > VOGL_MAX_POSSIBLE_HEAP_BLOCK_SIZE))
        {
            vogl_mem_error("vogl_new_array: Array too large!", pFile_line);
            return NULL;
        }

        uint8_t *q = static_cast<uint8_t *>(vogl_tracked_malloc(pFile_line, static_cast<size_t>(total)));

        T *p = reinterpret_cast<T *>(q + VOGL_MIN_ALLOC_ALIGNMENT);

        reinterpret_cast<uint32_t *>(p)[-1] = num;
        reinterpret_cast<uint32_t *>(p)[-2] = ~num;

        if (!VOGL_IS_SCALAR_TYPE(T))
        {
            helpers::construct_array(p, num);
        }

        return p;
    }

    template <typename T>
    inline void vogl_tracked_delete_array(const char *pFile_line, T *p)
    {
        if (p)
        {
            VOGL_ASSUME(VOGL_MIN_ALLOC_ALIGNMENT >= sizeof(uint32_t) * 2);
            VOGL_ASSERT((uint64_t)p >= sizeof(uint32_t) * 2);

            const uint32_t num = reinterpret_cast<uint32_t *>(p)[-1];
            const uint32_t num_check = reinterpret_cast<uint32_t *>(p)[-2];
            if ((num) && (num == ~num_check))
            {
                if (!VOGL_IS_SCALAR_TYPE(T))
                {
                    helpers::destruct_array(p, num);
                }

                vogl_tracked_free(pFile_line, reinterpret_cast<uint8_t *>(p) - VOGL_MIN_ALLOC_ALIGNMENT);
            }
            else
            {
                vogl_mem_error("Invalid ptr in call vogl_delete_array", pFile_line);
            }
        }
    }

    // Returns the actual size of the allocated block pointed to by p in BYTES, not objects.
    // p must have been allocated using vogl_new_array.
    inline size_t vogl_msize_array(void *p)
    {
        VOGL_ASSUME(VOGL_MIN_ALLOC_ALIGNMENT >= sizeof(uint32_t) * 2);
        VOGL_ASSERT((uint64_t)p >= sizeof(uint32_t) * 2);
        if (p)
        {
            const uint32_t num = reinterpret_cast<uint32_t *>(p)[-1];
            const uint32_t num_check = reinterpret_cast<uint32_t *>(p)[-2];
            if ((num) && (num == ~num_check))
            {
                size_t actual_size = vogl_msize(reinterpret_cast<uint8_t *>(p) - VOGL_MIN_ALLOC_ALIGNMENT);
                if (actual_size <= VOGL_MIN_ALLOC_ALIGNMENT)
                {
                    vogl_mem_error("vogl_msize() return value was too small in vogl_msize_array", VOGL_FILE_POS_STRING);
                    return 0;
                }
                return actual_size - VOGL_MIN_ALLOC_ALIGNMENT;
            }
            else
            {
                vogl_mem_error("Invalid ptr in call vogl_msize_array", VOGL_FILE_POS_STRING);
            }
        }

        return 0;
    }

} // namespace vogl

// These need to be outside of the namespace. The reason is that Visual Studio (as of 2013) will treat the define as part of the 
// namespace, but GCC and others do not. 
#define vogl_new(type, ...) vogl::vogl_tracked_new<type>(VOGL_MALLOC_FILE_LEN_STRING, ##__VA_ARGS__)
#define vogl_delete(p) vogl::vogl_tracked_delete(VOGL_MALLOC_FILE_LEN_STRING, p)

// num is limited to cUINT32_MAX
#define vogl_new_array(type, num) vogl::vogl_tracked_new_array<type>(VOGL_MALLOC_FILE_LEN_STRING, num)
#define vogl_delete_array(p) vogl::vogl_tracked_delete_array(VOGL_MALLOC_FILE_LEN_STRING, p)

#define VOGL_DEFINE_NEW_DELETE                                               \
    void *operator new(size_t size)                                            \
    {                                                                          \
        void *p = vogl_malloc(size);                                         \
        if (!p)                                                                \
            vogl_mem_error("new: Out of memory!", VOGL_FILE_POS_STRING);   \
        return p;                                                              \
    }                                                                          \
    void *operator new [](size_t size) { \
		void* p = vogl_malloc(size); \
      if (!p) \
			vogl_mem_error("new[]: Out of memory!", VOGL_FILE_POS_STRING); \
      return p; } void operator delete(void *p_block) \
    {                                                                          \
        vogl::vogl_free(p_block);                                          \
    }                                                                          \
    void operator delete [](void *p_block)                                     \
    { \
      vogl::vogl_free(p_block);                                   \
    }
