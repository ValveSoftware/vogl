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

// File: vogl_mem.cpp
#include "vogl_core.h"
#include "vogl_console.h"
#include "vogl.h"
#include "vogl_port.h"
#include "vogl_threading.h"
#include "vogl_strutils.h"

#if defined(PLATFORM_OSX)
	#include <malloc/malloc.h>
#else
	#include <malloc.h>
#endif

// Set to 1 to enable stb_malloc, otherwise voglcore uses plain malloc/free/realloc
#ifndef VOGL_USE_STB_MALLOC
#define VOGL_USE_STB_MALLOC 1
#endif

#ifndef VOGL_RAND_FILL_ALLLOCATED_MEMORY
#define VOGL_RAND_FILL_ALLLOCATED_MEMORY 0
#endif

#ifndef VOGL_SCRUB_FREED_MEMORY
#define VOGL_SCRUB_FREED_MEMORY 0
#endif

#if VOGL_RAND_FILL_ALLLOCATED_MEMORY || VOGL_SCRUB_FREED_MEMORY
#pragma message("VOGL_RAND_FILL_ALLLOCATED_MEMORY and/or VOGL_SCRUB_FREED_MEMORY is enabled.")
#endif

#ifdef VOGL_USE_WIN32_API
#include "vogl_winhdr.h"
#endif

#if defined(VOGL_USE_LINUX_API)
#include "mcheck.h"
#endif

#if VOGL_USE_STB_MALLOC
#include "vogl_stb_heap.h"
#endif

#if VOGL_MALLOC_DEBUGGING
#define MALLOC_DEBUG
#include "rmalloc.h"
#endif

#undef vogl_malloc
#undef vogl_realloc
#undef vogl_calloc
#undef vogl_free
#undef vogl_check_heap
#undef vogl_print_heap_stats
#undef vogl_new
#undef vogl_new_array
#undef vogl_delete
#undef vogl_delete_array

#if VOGL_MALLOC_DEBUGGING
#pragma message("vogl_mem.cpp: Malloc debugging enabled.")
#endif

#if VOGL_USE_STB_MALLOC

#define STB_ALLOC_INITIAL_HEAP_SIZE 32U * 1024U * 1024U
// Purposely written C-style so we can safely do heap allocs BEFORE C++ global construction time.

static uint64_t g_initial_heap_storage[(STBM_HEAP_SIZEOF + STB_ALLOC_INITIAL_HEAP_SIZE + sizeof(uint64_t) - 1) / sizeof(uint64_t)];
static stbm_heap *g_pHeap;
static pthread_mutex_t g_mutex;
static size_t g_page_size;

// This flag helps us detect calls to malloc, etc. from within asynchronous signal handlers.
static bool g_allocating_flag;

static void *sys_alloc(void *user_context, size_t size_requested, size_t *size_provided)
{
    VOGL_NOTE_UNUSED(user_context);

    if (!g_page_size)
    {
        g_page_size = static_cast<size_t>(plat_get_virtual_page_size());
        if (!g_page_size)
            g_page_size = 65536;
    }

    size_requested = (size_requested + g_page_size - 1) & (~(g_page_size - 1));

    return plat_virtual_alloc(size_requested, PLAT_READ | PLAT_WRITE, size_provided);
}

static void sys_free(void *user_context, void *ptr, size_t size)
{
    VOGL_NOTE_UNUSED(user_context);

    plat_virtual_free(ptr, size);
}

#if VOGL_MALLOC_DEBUGGING
static void *rmalloc_malloc_callback(size_t size, void *pUser)
{
    VOGL_NOTE_UNUSED(pUser);
    return stbm_alloc(NULL, g_pHeap, size, 0);
}

static void rmalloc_free_callback(void *ptr, void *pUser)
{
    VOGL_NOTE_UNUSED(pUser);
    stbm_free(NULL, g_pHeap, ptr);
}

static void *rmalloc_realloc_callback(void *ptr, size_t size, void *pUser)
{
    VOGL_NOTE_UNUSED(pUser);
    return stbm_realloc(NULL, g_pHeap, ptr, size, 0);
}
#endif

static void init_heap()
{
    if (g_pHeap)
        return;

    pthread_mutex_init(&g_mutex, NULL);

    stbm_heap_config config;
    memset(&config, 0, sizeof(config));
    config.system_alloc = sys_alloc;
    config.system_free = sys_free;

    g_pHeap = stbm_heap_init(g_initial_heap_storage, sizeof(g_initial_heap_storage), &config);

#if VOGL_MALLOC_DEBUGGING
    Rmalloc_set_callbacks(rmalloc_malloc_callback, rmalloc_free_callback, rmalloc_realloc_callback, NULL);
#endif
}

static void lock_heap()
{
    VOGL_ASSERT(g_pHeap);

    pthread_mutex_lock(&g_mutex);

    VOGL_ASSERT(!g_allocating_flag);
    g_allocating_flag = true;
}

static void unlock_heap()
{
    g_allocating_flag = false;

    VOGL_ASSERT(g_pHeap);
    pthread_mutex_unlock(&g_mutex);
}

static void *malloc_block(size_t size, const char *pFile_line)
{
    // If you hit this assert, it's most likely because vogl_core_init()
    //  (which calls vogl_init_heap) hasn't been called.
    VOGL_ASSERT(g_pHeap);

    lock_heap();

#if VOGL_MALLOC_DEBUGGING
    void *p = Rmalloc(size, pFile_line);
#else
    VOGL_NOTE_UNUSED(pFile_line);
    void *p = stbm_alloc(NULL, g_pHeap, size, 0);
#endif

    unlock_heap();

    return p;
}

static void *realloc_block(void *p, size_t size, const char *pFile_line)
{
    VOGL_ASSERT(g_pHeap);

    lock_heap();

#if VOGL_MALLOC_DEBUGGING
    void *q = Rrealloc(p, size, pFile_line);
#else
    VOGL_NOTE_UNUSED(pFile_line);
    void *q = stbm_realloc(NULL, g_pHeap, p, size, 0);
#endif

    unlock_heap();

    return q;
}

static void free_block(void *p, const char *pFile_line)
{
    VOGL_ASSERT(g_pHeap);

    lock_heap();

#if VOGL_MALLOC_DEBUGGING
    Rfree(p, pFile_line);
#else
    VOGL_NOTE_UNUSED(pFile_line);
    stbm_free(NULL, g_pHeap, p);
#endif

    unlock_heap();
}

static size_t msize_block(void *p, const char *pFile_line)
{
    VOGL_ASSERT(g_pHeap);

    lock_heap();

#if VOGL_MALLOC_DEBUGGING
    size_t n = Rmalloc_usable_size(p, pFile_line);
#else
    VOGL_NOTE_UNUSED(pFile_line);
    size_t n = stbm_get_allocation_size(p);
#endif

    unlock_heap();

    return n;
}

static void print_stats(const char *pFile_line)
{
    VOGL_ASSERT(g_pHeap);

#if VOGL_MALLOC_DEBUGGING
    lock_heap();

    Rmalloc_stat(pFile_line);

    unlock_heap();
#else
    VOGL_NOTE_UNUSED(pFile_line);
#endif
}

static void check(const char *pFile_line)
{
    VOGL_ASSERT(g_pHeap);

#if VOGL_MALLOC_DEBUGGING
    lock_heap();

    Rmalloc_test(pFile_line);

    unlock_heap();
#else
    VOGL_NOTE_UNUSED(pFile_line);
#endif
}

#else // !VOGL_USE_STB_MALLOC

static bool g_heap_initialized;
static pthread_mutex_t g_mutex;

static void init_heap()
{
    if (g_heap_initialized)
        return;

    pthread_mutex_init(&g_mutex, NULL);
    g_heap_initialized = true;
}

#if VOGL_USE_STB_MALLOC
static void lock_heap()
{
    VOGL_ASSERT(g_heap_initialized);
    pthread_mutex_lock(&g_mutex);
}

static void unlock_heap()
{
    VOGL_ASSERT(g_heap_initialized);
    pthread_mutex_unlock(&g_mutex);
}
#endif

static void *malloc_block(size_t size, const char *pFile_line)
{
    VOGL_ASSERT(g_heap_initialized);
    VOGL_NOTE_UNUSED(pFile_line);

#if VOGL_MALLOC_DEBUGGING
    lock_heap();
    void *p = Rmalloc(size, pFile_line);
    unlock_heap();
#else
    void *p = malloc(size);
#endif

    return p;
}

static void *realloc_block(void *p, size_t size, const char *pFile_line)
{
    VOGL_ASSERT(g_heap_initialized);
    VOGL_NOTE_UNUSED(pFile_line);

#if VOGL_MALLOC_DEBUGGING
    lock_heap();
    void *q = Rrealloc(p, size, pFile_line);
    unlock_heap();
#else
    void *q = realloc(p, size);
#endif

    return q;
}

static void free_block(void *p, const char *pFile_line)
{
    VOGL_ASSERT(g_heap_initialized);
    VOGL_NOTE_UNUSED(pFile_line);

#if VOGL_MALLOC_DEBUGGING
    lock_heap();
    Rfree(p, pFile_line);
    unlock_heap();
#else
    free(p);
#endif
}

static size_t msize_block(void *p, const char *pFile_line)
{
    VOGL_ASSERT(g_heap_initialized);
    VOGL_NOTE_UNUSED(pFile_line);

#if VOGL_MALLOC_DEBUGGING
    lock_heap();
    size_t n = Rmalloc_usable_size(p, pFile_line);
    unlock_heap();
#else
    size_t n = malloc_usable_size(p);
#endif

    return n;
}

static void print_stats(const char *pFile_line)
{
    VOGL_ASSERT(g_heap_initialized);
    VOGL_NOTE_UNUSED(pFile_line);

#if VOGL_MALLOC_DEBUGGING
    lock_heap();
    Rmalloc_stat(pFile_line);
    unlock_heap();
#endif
}

static void check(const char *pFile_line)
{
    VOGL_ASSERT(g_heap_initialized);
    VOGL_NOTE_UNUSED(pFile_line);

#if VOGL_MALLOC_DEBUGGING
    lock_heap();
    Rmalloc_test(pFile_line);
    unlock_heap();
#endif
}

#endif // #ifdef VOGL_USE_STB_MALLOC

namespace vogl
{

void vogl_init_heap()
{
    init_heap();
}

// TODO: vararg this
void vogl_mem_error(const char *pMsg, const char *pFile_line)
{
    char buf[512];
    vogl::vogl_sprintf_s(buf, sizeof(buf), "%s: Fatal error: %s. Originally called from %s.\n", VOGL_FUNCTION_INFO_CSTR, pMsg, pFile_line ? pFile_line : "?");
    vogl_fail(buf, __FILE__, __LINE__);
    abort();
}

#if VOGL_RAND_FILL_ALLLOCATED_MEMORY || VOGL_SCRUB_FREED_MEMORY
static uint32_t g_cur_rand = 0xDEADBEEF;

static void random_fill(void *p, size_t size)
{
#define JSR (jsr ^= (jsr << 17), jsr ^= (jsr >> 13), jsr ^= (jsr << 5));
    uint32_t jsr = g_cur_rand;

    while (size >= sizeof(uint32_t) * 4)
    {
        static_cast<uint32_t *>(p)[0] = jsr;
        JSR;
        static_cast<uint32_t *>(p)[1] = jsr;
        JSR;
        static_cast<uint32_t *>(p)[2] = jsr;
        JSR;
        static_cast<uint32_t *>(p)[3] = jsr;
        JSR;

        size -= sizeof(uint32_t) * 4;
        p = static_cast<uint32_t *>(p) + 4;
    }

    while (size >= sizeof(uint32_t))
    {
        static_cast<uint32_t *>(p)[0] = jsr;
        JSR;

        size -= sizeof(uint32_t);
        p = static_cast<uint32_t *>(p);
    }

    while (size)
    {
        static_cast<uint8_t *>(p)[0] = static_cast<uint8_t>(jsr);
        JSR;

        size--;
        p = static_cast<uint8_t *>(p) + 1;
    }

    g_cur_rand = jsr;
#undef JSR
}
#endif // VOGL_RAND_FILL_ALLLOCATED_MEMORY || VOGL_SCRUB_FREED_MEMORY

void *vogl_tracked_malloc(const char *pFile_line, size_t size, size_t *pActual_size)
{
    size = (size + sizeof(uint32_t) - 1U) & ~(sizeof(uint32_t) - 1U);
    if (!size)
        size = sizeof(uint32_t);

    if (size > VOGL_MAX_POSSIBLE_HEAP_BLOCK_SIZE)
    {
        vogl_mem_error("vogl_malloc: size too big", pFile_line);
        return NULL;
    }

    uint8_t *p_new = (uint8_t *)malloc_block(size, pFile_line);

    VOGL_ASSERT((reinterpret_cast<uintptr_t>(p_new) & (VOGL_MIN_ALLOC_ALIGNMENT - 1)) == 0);

    if (!p_new)
    {
        vogl_mem_error("vogl_malloc: out of memory", pFile_line);
        return NULL;
    }

    if (pActual_size)
    {
        *pActual_size = msize_block(p_new, pFile_line);

        if ((size) && (*pActual_size >= static_cast<uint64_t>(size) * 16U))
        {
            // I've seen this happen with glibc's absolutely terrible debug heap crap, best to let the caller know that shit is going to die
            fprintf(stderr, "%s: malloc_usable_size may be misbehaving! Requested %" PRIuPTR " bytes, but the usable size is reported as %" PRIuPTR " bytes.\n",
                    __FUNCTION__, size, *pActual_size);
        }
    }

#if VOGL_RAND_FILL_ALLLOCATED_MEMORY
    random_fill(p_new, size);
#endif

    return p_new;
}

void *vogl_tracked_realloc(const char *pFile_line, void *p, size_t size, size_t *pActual_size)
{
    if ((uintptr_t)p & (VOGL_MIN_ALLOC_ALIGNMENT - 1))
    {
        vogl_mem_error("vogl_realloc: bad ptr", pFile_line);
        return NULL;
    }

    if (size > VOGL_MAX_POSSIBLE_HEAP_BLOCK_SIZE)
    {
        vogl_mem_error("vogl_realloc: size too big!", pFile_line);
        return NULL;
    }

    if ((size) && (size < sizeof(uint32_t)))
        size = sizeof(uint32_t);

#if VOGL_RAND_FILL_ALLLOCATED_MEMORY || VOGL_SCRUB_FREED_MEMORY
    size_t orig_size = p ? msize_block(p, pFile_line) : 0;

#if VOGL_SCRUB_FREED_MEMORY
    if ((orig_size) && (!size))
        random_fill(p, orig_size);
#endif
#endif

    void *p_new = realloc_block(p, size, pFile_line);

    VOGL_ASSERT((reinterpret_cast<uintptr_t>(p_new) & (VOGL_MIN_ALLOC_ALIGNMENT - 1)) == 0);

    if (pActual_size)
    {
        *pActual_size = 0;

        if (size)
        {
            if (p_new)
                *pActual_size = msize_block(p_new, pFile_line);
            else if (p)
                *pActual_size = msize_block(p, pFile_line);

            if (*pActual_size >= static_cast<uint64_t>(size) * 16U)
            {
                // I've seen this happen with glibc's absolutely terrible debug heap crap, best to let the caller know that shit is going to die
                fprintf(stderr, "%s: malloc_usable_size may be misbehaving! Requested %" PRIuPTR " bytes, but the usable size is reported as %" PRIuPTR " bytes.\n",
                        __FUNCTION__, size, *pActual_size);
            }
        }
    }

#if VOGL_RAND_FILL_ALLLOCATED_MEMORY
    if ((size) && (p_new))
    {
        size_t new_size = msize_block(p_new, pFile_line);

        if (new_size > orig_size)
            random_fill(static_cast<uint8_t *>(p_new) + orig_size, new_size - orig_size);
    }
#endif

    return p_new;
}

void *vogl_tracked_calloc(const char *pFile_line, size_t count, size_t size, size_t *pActual_size)
{
    size_t total = count * size;
    void *p = vogl_tracked_malloc(pFile_line, total, pActual_size);
    if (p)
        memset(p, 0, total);
    return p;
}

void vogl_tracked_free(const char *pFile_line, void *p)
{
    if (!p)
        return;

    if (reinterpret_cast<uintptr_t>(p) & (VOGL_MIN_ALLOC_ALIGNMENT - 1))
    {
        vogl_mem_error("vogl_free: bad ptr", pFile_line);
        return;
    }

#if VOGL_SCRUB_FREED_MEMORY
    random_fill(p, msize_block(p, pFile_line));
#endif

    free_block(p, pFile_line);
}

size_t vogl_msize(void *p)
{
    if (!p)
        return 0;

    if (reinterpret_cast<uintptr_t>(p) & (VOGL_MIN_ALLOC_ALIGNMENT - 1))
    {
        vogl_mem_error("vogl_msize: bad ptr", VOGL_FILE_POS_STRING);
        return 0;
    }

    return msize_block(p, VOGL_FILE_POS_STRING);
}

void vogl_tracked_print_stats(const char *pFile_line)
{
    print_stats(pFile_line);
}

void vogl_tracked_check_heap(const char *pFile_line)
{
    check(pFile_line);

#if defined(COMPILER_MSVC)
    // Also see_CrtSetDbgFlag ()
    int status = _CrtCheckMemory();
    VOGL_VERIFY(status == TRUE);
#elif defined(VOGL_USE_LINUX_API)
    // App MUST have been linked with -mlcheck! Unfortunately -mlcheck() causes the CRT's heap API's to be unusable on some dev boxes, no idea why.
    mcheck_check_all();
#elif defined(VOGL_USE_OSX_API)
	VOGL_VERIFY(malloc_zone_check(NULL));
#else
    fprintf(stderr, "%s: Need implementation\n", __FUNCTION__);
#endif
}

} // namespace vogl

extern "C" void *vogl_realloc(const char *pFile_line, void *p, size_t new_size)
{
    return vogl::vogl_tracked_realloc(pFile_line, p, new_size, NULL);
}
