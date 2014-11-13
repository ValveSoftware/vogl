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

// File: vogl_port_posix.cpp

#if !defined(PLATFORM_POSIX)
    #error "vogl_port_posix should not be compiled on non-POSIX platforms."
#endif

#include "vogl_port.h"
#include <dlfcn.h>
#include <fcntl.h>
#include <paths.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <sys/time.h>

#if defined(PLATFORM_OSX)
	#include <unistd.h>
#endif

int plat_chdir(const char* path)
{
    return chdir(path);
}

char* plat_getcwd(char *buffer, int maxlen)
{
    return getcwd(buffer, maxlen);
}

const char* plat_gettmpdir()
{
    static char s_tmpdir[PATH_MAX];

    if (!s_tmpdir[0])
    {
        const char *tmpdir = getenv("TMPDIR");

        if (!tmpdir)
        {
            tmpdir = P_tmpdir;
            if (!tmpdir)
                tmpdir = _PATH_TMP;
        }

        strncpy(s_tmpdir, tmpdir, sizeof(s_tmpdir));
        s_tmpdir[sizeof(s_tmpdir) - 1] = 0;

        // Remove trailing slash.
        size_t slen = strlen(s_tmpdir);
        if ((slen > 0) && (s_tmpdir[slen - 1] == '/'))
            s_tmpdir[--slen] = 0;
    }

    return s_tmpdir;
}

char* plat_gettmpfname(char *buffer, int maxlen, const char *prefix)
{
    struct timeval cur_time;
    uint32_t rnd32 = plat_rand();
    const char *tmpdir = plat_gettmpdir();

    gettimeofday(&cur_time, NULL);

    uint64_t time64 = cur_time.tv_sec * 1000000ULL + cur_time.tv_usec;

    // Create a fileiname with THREADID & RAND32 & TIME64.
    snprintf(buffer, maxlen, "%s/_%s_%x_%x_%" PRIx64 ".tmp", tmpdir, prefix, plat_gettid(), rnd32, time64);
    buffer[maxlen - 1] = 0;
    return buffer;
}

// Tests for the existence of the specified path, returns true if it exists and false otherwise.
bool plat_fexist(const char* path)
{
    return access(path, F_OK) == 0;
}

pid_t plat_gettid()
{
    return static_cast<pid_t>(syscall(SYS_gettid));
}

uint64_t plat_posix_gettid()
{
    return reinterpret_cast<uintptr_t>(pthread_self());
}

pid_t plat_getpid()
{
    return getpid();
}

pid_t plat_getppid()
{
    return getppid();
}

size_t plat_rand_s(uint32_t* out_array, size_t out_array_length)
{
    static __thread unsigned int s_seed = 0;

    if (s_seed == 0)
    {
        // Try to seed rand_r() with /dev/urandom.
        ssize_t nbytes = 0;
        int fd = open("/dev/urandom", O_RDONLY);
        if (fd != -1)
        {
            nbytes = read(fd, &s_seed, sizeof(s_seed));
            close(fd);
        }

        // If that didn't work, fallback to time and thread id.
        if (nbytes != sizeof(s_seed))
        {
            struct timeval time;
            gettimeofday(&time, NULL);
            s_seed = plat_gettid() ^ ((time.tv_sec * 1000) + (time.tv_usec / 1000));
        }
    }

    for (size_t i = 0; i < out_array_length; ++i)
        out_array[i] = rand_r(&s_seed);

    return out_array_length;
}

uint32_t plat_rand()
{
    uint32_t num;
    plat_rand_s(&num, 1);
    return num;
}

// Returns the size of a virtual page of memory.
int32_t plat_get_virtual_page_size()
{
    return sysconf(_SC_PAGE_SIZE);
}

static int get_prot_from_access(uint32_t access_flags)
{
    int ret_flags = 0;
    if ((access_flags & PLAT_WRITE) == PLAT_WRITE)
    {
        ret_flags |= PROT_WRITE;
        access_flags &= ~PLAT_WRITE;
    }

    if ((access_flags & (PLAT_READ)) == (PLAT_READ))
    {
        ret_flags |= PROT_READ;
        access_flags &= ~(PLAT_READ);
    }

    // If this fires, it means the code above needs to be updated to support the flag being passed in.
    VOGL_VERIFY(access_flags == 0);

    return ret_flags;
}


void* plat_virtual_alloc(size_t size_requested, uint32_t access_flags, size_t* out_size_provided)
{
    const int prot = get_prot_from_access(access_flags);
    const int flags = MAP_ANON | MAP_SHARED;
    void *p = mmap(NULL, size_requested, prot, flags, -1, 0);

    if ((!p) || (p == MAP_FAILED))
    {
        uint32_t e = errno;

        // Not using strerror_r because it fails when we're completely out of memory.
        char *pError_desc = strerror(e);

        char buf[256];
        snprintf(buf, sizeof(buf), "%s: mmap() of %" PRIuPTR " bytes failed! Reason: %s (errno 0x%x)\n",
                 VOGL_FUNCTION_INFO_CSTR, size_requested, pError_desc, e);
        buf[sizeof(buf) - 1] = 0;

        write(STDERR_FILENO, buf, strlen(buf));
        abort();
    }

    *out_size_provided = size_requested;

    return p;
}

void plat_virtual_free(void* free_addr, size_t size)
{
    int res = munmap(free_addr, size);
    if (res != 0)
    {
        uint32_t e = errno;

        // Not using strerror_r because it fails when we're completely out of memory.
        char *pError_desc = strerror(e);

        char buf[256];
        snprintf(buf, sizeof(buf), "%s: munmap() free_addr 0x%" PRIxPTR " size 0x%" PRIxPTR " failed! Reason: %s (errno 0x%x)\n",
                 VOGL_FUNCTION_INFO_CSTR, (uintptr_t)free_addr, size, pError_desc, e);
        buf[sizeof(buf) - 1] = 0;

        write(STDERR_FILENO, buf, strlen(buf));
        abort();
    }
}

#if VOGL_USE_PTHREADS_API
    int plat_sem_post(sem_t* sem, uint32_t release_count)
    {
        int status = 0;
        while (release_count > 0)
        {
            status = sem_post(sem);
            if (status)
                break;
            release_count--;
        }

        return status;
    }

    void plat_try_sem_post(sem_t* sem, uint32_t release_count)
    {
        while (release_count > 0)
        {
            sem_post(sem);
            release_count--;
        }
    }
#endif

void* plat_dlsym(void* handle, const char* symbol)
{
    return dlsym(handle, symbol);
}

void* plat_load_system_gl(int _flags)
{
    return dlopen(plat_get_system_gl_module_name(), _flags);
}
