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

// File: vogl_stb_heap.h
#pragma once

#include "vogl_core.h"

#include <sys/types.h>

#ifdef PLATFORM_POSIX
	#include <sys/mman.h>
	#include <err.h>
	#include <fcntl.h>
	#include <unistd.h>
#endif

#include "stb_malloc.h"

namespace vogl
{

// A really basic STB heap that uses mmap() for sys allocs. Uses its own locking for max safety until I can be 100% sure that stb's locking is robust.
// Note, this class is not actually used for anything right now (vogl_mem.cpp just calls stb_malloc directory).
template <uint32_t initialize_size, bool locking, bool destroy>
class stb_heap
{
    VOGL_NO_COPY_OR_ASSIGNMENT_OP(stb_heap);

public:
    stb_heap()
#if defined(VOGL_USE_OSX_API)
        : m_page_size(sysconf(_SC_PAGESIZE))
#else
        : m_page_size(sysconf(_SC_PAGE_SIZE))
#endif
    {
        stbm_heap_config config;
        memset(&config, 0, sizeof(config));
        config.system_alloc = sys_alloc;
        config.system_free = sys_free;
        config.user_context = this;

        m_pHeap = stbm_heap_init(m_initial_heap_storage, sizeof(m_initial_heap_storage), &config);
    }

    ~stb_heap()
    {
        if (destroy)
        {
            stbm_heap_free(m_pHeap);
        }
    }

    stbm_heap *get_stb_heap()
    {
        return m_pHeap;
    }

    void lock()
    {
        if (locking)
            m_mutex.lock();
    }

    void unlock()
    {
        if (locking)
            m_mutex.unlock();
    }

    // Purposely not named malloc, etc. in case they are macros.
    void *malloc_block(size_t size)
    {
        lock();
        void *p = stbm_alloc(NULL, m_pHeap, size, 0);
        unlock();
        return p;
    }

    void *realloc_block(void *p, size_t size)
    {
        lock();
        void *q = stbm_realloc(NULL, m_pHeap, p, size, 0);
        unlock();
        return q;
    }

    void free_block(void *p)
    {
        lock();
        stbm_free(NULL, m_pHeap, p);
        unlock();
    }

    size_t msize_block(void *p)
    {
        lock();
        size_t n = stbm_get_allocation_size(p);
        unlock();
        return n;
    }

private:
    stbm_heap *m_pHeap;
    vogl::mutex m_mutex;
    size_t m_page_size;
    uint64_t m_initial_heap_storage[(STBM_HEAP_SIZEOF + initialize_size + sizeof(uint64_t) - 1) / sizeof(uint64_t)];

    static void *sys_alloc(void *user_context, size_t size_requested, size_t *size_provided)
    {
        stb_heap *pHeap = static_cast<stb_heap *>(user_context);

        size_requested = (size_requested + pHeap->m_page_size - 1) & (~(pHeap->m_page_size - 1));

        void *p = mmap(NULL, size_requested, PROT_READ | PROT_WRITE, MAP_ANON | MAP_SHARED, -1, 0);

        *size_provided = 0;

        if (p)
        {
            *size_provided = size_requested;
        }

        return p;
    }

    static void sys_free(void *user_context, void *ptr, size_t size)
    {
        VOGL_NOTE_UNUSED(user_context);

        int res = munmap(ptr, size);
        if (res != 0)
        {
            VOGL_FAIL("sys_free: munmap() failed!\n");
        }
    }
};

} // namespace vogl
