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

// File: vogl_dynamic_module.h
//
// TODO: Add Linux support!
//
#pragma once

#include "vogl_core.h"

namespace vogl
{
    typedef void *dynamic_module_handle_t;

    dynamic_module_handle_t load_dynamic_module(const char *pModule_filename);
    void unload_dynamic_module(dynamic_module_handle_t handle);
    void *get_dynamic_module_proc_address(dynamic_module_handle_t handle, const char *pFunc_name);
    void *lookup_dynamic_module_func(const char *pModule_filename, const char *pFunc_name);

    class dynamic_module_cache
    {
    public:
        dynamic_module_cache();
        ~dynamic_module_cache();

        void unload_all();

        bool is_module_loaded(const char *pModule_filename);
        dynamic_module_handle_t get_module(const char *pModule_filename);

        static dynamic_module_cache &get_global()
        {
            static dynamic_module_cache s_module_cache;
            return s_module_cache;
        }

    private:
        struct loaded_module
        {
            dynamic_string m_filename;
            dynamic_module_handle_t m_handle;
        };

        vogl::vector<loaded_module> m_modules;
    };

    template <typename dynamic_func_def_t>
    class dynamic_function
    {
    public:
        dynamic_function()
            : m_pFunc(NULL)
        {
        }
        dynamic_function(const char *pModule_filename, const char *pFunc_name)
            : m_pFunc(NULL)
        {
            lookup(pModule_filename, pFunc_name);
        }

        inline void clear()
        {
            m_pFunc = NULL;
        }

        inline dynamic_func_def_t get_ptr() const
        {
            return m_pFunc;
        }
        inline operator dynamic_func_def_t() const
        {
            return m_pFunc;
        }

        inline void set(dynamic_func_def_t ptr)
        {
            m_pFunc = ptr;
        }

        inline operator bool() const
        {
            return m_pFunc != NULL;
        }
        inline bool operator!() const
        {
            return m_pFunc == NULL;
        }

        inline bool lookup(const char *pModule_filename, const char *pFunc_name)
        {
            if (!m_pFunc)
                m_pFunc = (dynamic_func_def_t)lookup_dynamic_module_func(pModule_filename, pFunc_name);
            return m_pFunc != NULL;
        }

    private:
        dynamic_func_def_t m_pFunc;
    };

} // namespace vogl
