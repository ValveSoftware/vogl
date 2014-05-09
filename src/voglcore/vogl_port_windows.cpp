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

// File: vogl_port_windows.cpp
#include "vogl_port.h"
#include <io.h>
#include <direct.h>

#if !defined(PLATFORM_WINDOWS)
    #error "vogl_port_windows should not be compiled on non-windows platforms."
#endif

#if !defined(VOGL_USE_WIN32_API)
    #error "Many functions in this file require the Win32 API."
#endif

int plat_chdir(const char* path)
{
    return _chdir(path);
}

char* plat_getcwd(char *buffer, int maxlen)
{
    return _getcwd(buffer, maxlen);
}

const char* plat_gettmpdir()
{
    static char s_tmpdir[MAX_PATH];

    if (!s_tmpdir[0])
    {
        DWORD ret = GetTempPath(MAX_PATH, s_tmpdir);

        if ((ret == 0) || (ret > MAX_PATH))
        {
            // Ugh. Try several environment variables and just
            //  set to c:\\temp if all those fail.
            const char *tmpdir = getenv("TMP");
            if (!tmpdir)
            {
                tmpdir = getenv("TEMP");
                if (!tmpdir)
                {
                    tmpdir = getenv("USERPROFILE");
                    if (!tmpdir)
                        tmpdir = "c:\\temp";
                }
            }

            strncpy(s_tmpdir, tmpdir, sizeof(s_tmpdir));
            s_tmpdir[sizeof(s_tmpdir) - 1] = 0;
        }

        // Remove trailing slash.
        size_t slen = strlen(s_tmpdir);
        if ((slen > 0) && (s_tmpdir[slen - 1] == '\\'))
            s_tmpdir[--slen] = 0;
    }

    return s_tmpdir;
}

char* plat_gettmpfname(char *buffer, int maxlen, const char *prefix)
{
    const char *tmpdir = plat_gettmpdir();
    uint32_t rnd32 = plat_rand();
    unsigned int time32 = (unsigned int)time(NULL);

    _snprintf(buffer, maxlen, "%s\\_%s_%08x_%08x.tmp", tmpdir, prefix, time32, rnd32);
    buffer[maxlen - 1] = 0;
    return buffer;
}

bool plat_fexist(const char* path)
{
    return _access(path, 00) == 0;
}

pid_t plat_gettid()
{
    return static_cast<pid_t>(GetCurrentThreadId());
}

uint64_t plat_posix_gettid()
{
    // pthread_self on windows doesn't return something we can trivially cast (annoyingly).
    return static_cast<uint64_t>(GetCurrentThreadId());
}


pid_t plat_getpid()
{
    return static_cast<pid_t>(GetCurrentProcessId());
}

size_t plat_rand_s(uint32_t* out_array, size_t out_array_length)
{
    VOGL_ASSUME(sizeof(uint32_t) == sizeof(unsigned int));

    size_t ret_values = 0;
    for (ret_values = 0; ret_values < out_array_length; ++ret_values)
    {
        if (FAILED(rand_s(&out_array[ret_values]))) 
            return ret_values;
    }

    return ret_values;
}

uint32_t plat_rand()
{
    uint32_t num;
    return (plat_rand_s(&num, 1) > 0) ? num : rand();
}

// Returns the size of a virtual page of memory.
int32_t plat_get_virtual_page_size()
{
    SYSTEM_INFO sys_info;
    GetSystemInfo(&sys_info);

    return sys_info.dwPageSize;
}

static DWORD get_alloc_flags_from_access(uint32_t access_flags)
{
    // Windows flags are specified here: http://msdn.microsoft.com/en-us/library/windows/desktop/aa366786(v=vs.85).aspx
    // It's not possible to write memory without also reading memory. This is true generally, but also windows doesn't
    // have a way to specify flags that would be 'write-only.'
    VOGL_ASSERT((access_flags & PLAT_WRITE) == 0 || (access_flags & PLAT_READ) != 0);

    // Windows flags for this are kinda dumb.
    DWORD ret_flags = 0;
    if ((access_flags & (PLAT_READ | PLAT_WRITE)) == (PLAT_READ | PLAT_WRITE)) 
    {
        ret_flags |= PAGE_READWRITE;
        access_flags &= ~(PLAT_READ | PLAT_WRITE);
    }

    if ((access_flags & PLAT_READ) == PLAT_READ) 
    {
        ret_flags |= PAGE_READONLY;
        access_flags &= ~PLAT_READ;    
    }

    // If this fires, it means the code above needs to be updated to support the flag being passed in.
    VOGL_VERIFY(access_flags == 0);

    return ret_flags;
}

void* plat_virtual_alloc(size_t size_requested, uint32_t access_flags, size_t* out_size_provided)
{
    const DWORD alloc_flags = get_alloc_flags_from_access(access_flags);
    
    void* ret_ptr = VirtualAlloc(NULL, size_requested, MEM_COMMIT|MEM_RESERVE, alloc_flags);
    // There's a somewhat complex way to ask for this, but we'll skip it for now and just say that
    // the size requested was the size granted. 

    if (!ret_ptr)
    {
        VOGL_VERIFY(!"Failed to allocate a virtual page, a debug session is needed.");
        char buf[256];
        sprintf(buf, "VirtualAlloc failed. TODO: Better messaging!");
        fwrite(buf, strlen(buf), 1, stderr);
        abort();
    }

    (*out_size_provided) = size_requested;

    return ret_ptr;
}

void plat_virtual_free(void* free_addr, size_t size)
{
    VOGL_NOTE_UNUSED(size);

    if (VirtualFree(free_addr, 0, MEM_RELEASE) == FALSE)
    {
        VOGL_VERIFY(!"Failed to deallocate a system page. A debug session is needed.");
        char buf[256];
        sprintf(buf, "VirtualFree failed. TODO: Better messaging!");
        fwrite(buf, strlen(buf), 1, stderr);
        abort();
    }
}

#if VOGL_USE_PTHREADS_API
    int plat_sem_post(sem_t* sem, uint32_t release_count)
    {
        if (1 == release_count)
            return sem_post(sem);

        return sem_post_multiple(sem, release_count);
    }

    void plat_try_sem_post(sem_t* sem, uint32_t release_count)
    {
        if (1 == release_count)
            sem_post(sem);
        else
            sem_post_multiple(sem, release_count);
    }

#endif

HMODULE _load_opengl32_dll()
{
    char system_directory_root[_MAX_PATH];
    char module_name[_MAX_PATH];

    // These are basically backwards. GG Microsoft.
    #if defined(PLATFORM_64BIT)
        GetSystemDirectoryA(system_directory_root, VOGL_ARRAY_SIZE(system_directory_root));
    #else
        // On 32-bit windows (which apparently some people still use), the SysWow64 directory call will fail
        // and we can use that cue to go ask for the regular ol' 32-bit variant.
        if (GetSystemWow64DirectoryA(system_directory_root, VOGL_ARRAY_SIZE(system_directory_root)) == 0)
            GetSystemDirectoryA(system_directory_root, VOGL_ARRAY_SIZE(system_directory_root));
    #endif

    sprintf_s(module_name, VOGL_ARRAY_SIZE(module_name), "%s\\%s", system_directory_root, plat_get_system_gl_module_name());
    
    HMODULE retModule = LoadLibraryEx(module_name, NULL, 0);
    return retModule;
}

void* plat_dlsym(void* handle, const char* symbol)
{
    if (handle == PLAT_RTLD_NEXT)
    {
        // TODO: This leaks, we shouldn't leak.
        static HMODULE hOpenGL32 = _load_opengl32_dll();
        handle = hOpenGL32;
    }

    void* func = GetProcAddress((HMODULE)handle, symbol);
    return func;
}

void* plat_load_system_gl(int _flags)
{
    VOGL_NOTE_UNUSED(_flags);

    return (void*)_load_opengl32_dll();
}
