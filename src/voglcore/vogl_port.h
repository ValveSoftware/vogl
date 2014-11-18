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

// Change the working directory, return 0 on success and -1 on failure.
int plat_chdir(const char* path);

// Get the cwd, store into buffer. Retun buffer on success or NULL on error.
char* plat_getcwd(char *buffer, int maxlen);

// Get the temporary directory.
const char* plat_gettmpdir();

// Get a full path to a temporary filename prefixed with prefix.
char* plat_gettmpfname(char *buffer, int maxlen, const char *prefix);

// Tests for the existence of the specified path, returns true if it exists and false otherwise.
bool plat_fexist(const char* path);

// Get the thread id for this thread.
pid_t plat_gettid();

// Get the posix thread id (which may be the same as above--but they're different in source so different here)
uint64_t plat_posix_gettid();

// Get the process id for this process.
pid_t plat_getpid();

// Provides out_array_length uint32s of random data from a secure service (/dev/urandom, crypto apis, etc).
size_t plat_rand_s(uint32_t* out_array, size_t out_array_length);

// Provide a single random uint32_t.
uint32_t plat_rand();

// Virtual memory handling
#define PLAT_READ 0x01
#define PLAT_WRITE 0x02

// Returns the size of a virtual page of memory.
int32_t plat_get_virtual_page_size();

void* plat_virtual_alloc(size_t size_requested, uint32_t access_flags, size_t* out_size_provided);
void plat_virtual_free(void* free_addr, size_t size);

#if VOGL_USE_PTHREADS_API
    int plat_sem_post(sem_t* sem, uint32_t release_count);
    void plat_try_sem_post(sem_t* sem, uint32_t release_count);
#endif

#if defined(PLATFORM_WINDOWS)
    #define PLAT_RTLD_NEXT ((void*)0)
    #define PLAT_RTLD_LAZY ((int)0)
#elif defined(PLATFORM_POSIX)
    #define PLAT_RTLD_NEXT (RTLD_NEXT)
    #define PLAT_RTLD_LAZY (RTLD_LAZY)
#endif

#if defined(PLATFORM_LINUX)
    inline const char* plat_get_system_gl_module_name() { return "libGL.so.1"; }
#elif defined(PLATFORM_OSX)
    inline const char* plat_get_system_gl_module_name() { return "OpenGL"; }
#elif defined(PLATFORM_WINDOWS)
    inline const char* plat_get_system_gl_module_name() { return "opengl32.dll"; }
#else
    #error "Need to define plat_get_system_gl_module_name for this platform."
#endif

void* plat_dlsym(void* handle, const char* symbol);

void* plat_load_system_gl(int _flags);
