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

// File: vogl_dynamic_module.cpp
//
// TODO: Add Linux support!
//
#include "vogl_core.h"
#include "vogl_dynamic_module.h"

#ifdef VOGL_USE_WIN32_API
#include "vogl_winhdr.h"
#endif

namespace vogl
{
    dynamic_module_handle_t load_dynamic_module(const char *pFilename)
    {
#if defined(VOGL_ANSI_CPLUSPLUS)
        VOGL_NOTE_UNUSED(pFilename);
        return NULL;
#elif defined(VOGL_USE_WIN32_API)
        return (dynamic_module_handle_t)LoadLibraryA(pFilename);
#else
        VOGL_NOTE_UNUSED(pFilename);
        printf("Implement me: load_dynamic_module()\n");
        return NULL;
#endif
    }

    void unload_dynamic_module(dynamic_module_handle_t handle)
    {
#if defined(VOGL_ANSI_CPLUSPLUS)
        VOGL_NOTE_UNUSED(handle);
#elif defined(VOGL_USE_WIN32_API)
        if (handle)
        {
            FreeLibrary((HMODULE)handle);
        }
#else
        VOGL_NOTE_UNUSED(handle);
        printf("Implement me: unload_dynamic_module()\n");
#endif
    }

    void *get_dynamic_module_proc_address(dynamic_module_handle_t handle, const char *pFunc_name)
    {
        if (!handle)
            return NULL;
#if defined(VOGL_ANSI_CPLUSPLUS)
        VOGL_NOTE_UNUSED(handle);
        VOGL_NOTE_UNUSED(pFunc_name);
        return NULL;
#elif defined(VOGL_USE_WIN32_API)
        VOGL_NOTE_UNUSED(handle);
        return GetProcAddress((HMODULE)handle, pFunc_name);
#else
        VOGL_NOTE_UNUSED(handle);
        VOGL_NOTE_UNUSED(pFunc_name);
        printf("Implement me: get_dynamic_module_proc_address()\n");
        return NULL;
#endif
    }

    dynamic_module_cache::dynamic_module_cache()
    {
    }

    dynamic_module_cache::~dynamic_module_cache()
    {
    }

    void dynamic_module_cache::unload_all()
    {
        for (uint32_t i = 0; i < m_modules.size(); i++)
            unload_dynamic_module(m_modules[i].m_handle);
        m_modules.clear();
    }

    bool dynamic_module_cache::is_module_loaded(const char *pFilename)
    {
        for (uint32_t i = 0; i < m_modules.size(); i++)
            if (m_modules[i].m_filename == pFilename)
                return true;
        return false;
    }

    dynamic_module_handle_t dynamic_module_cache::get_module(const char *pFilename)
    {
        for (uint32_t i = 0; i < m_modules.size(); i++)
            if (m_modules[i].m_filename == pFilename)
                return m_modules[i].m_handle;

        dynamic_module_handle_t handle = load_dynamic_module(pFilename);
        if (!handle)
            return NULL;

        loaded_module *pModule = m_modules.enlarge(1);
        pModule->m_filename = pFilename;
        pModule->m_handle = handle;
        return handle;
    }

    void *lookup_dynamic_module_func(const char *pModule_filename, const char *pFunc_name)
    {
        dynamic_module_handle_t mod_handle = dynamic_module_cache::get_global().get_module(pModule_filename);
        if (!mod_handle)
            return NULL;

        return get_dynamic_module_proc_address(mod_handle, pFunc_name);
    }

} // namespace vogl
