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

// File: vogl_port.h
// Purpose: This file contains the prototypes for functions that one platform or the other expect
// The guts of those functions live in the platform-specific CPP files.
#pragma once

#include "vogl_core.h"

// Get the thread id for this thread.
pid_t plat_gettid();

// Get the posix thread id (which may be the same as above--but they're different in source so different here)
uint64_t plat_posix_gettid();

// Get the process id for this process.
pid_t plat_getpid();

// Get the process id of the parent process.
pid_t plat_getppid();

// Provides out_array_length uint32s of random data from a secure service (/dev/urandom, crypto apis, etc).
size_t plat_rand_s(vogl::uint32* out_array, size_t out_array_length);

// Virtual memory handling
#define PLAT_READ 0x01
#define PLAT_WRITE 0x02

// Returns the size of a virtual page of memory.
vogl::int32 plat_get_virtual_page_size();

void* plat_virtual_alloc(size_t size_requested, vogl::uint32 access_flags, size_t* out_size_provided);
void plat_virtual_free(void* free_addr, size_t size);

#if VOGL_USE_PTHREADS_API
    int plat_sem_post(sem_t* sem, vogl::uint32 release_count);
    void plat_try_sem_post(sem_t* sem, vogl::uint32 release_count);
#endif
