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

// File: vogl_win32_find_files.h
#pragma once

#include "vogl_core.h"

namespace vogl
{
    class find_files
    {
    public:
        struct file_desc
        {
            inline file_desc()
                : m_is_dir(false)
            {
            }

            dynamic_string m_fullname; // full filename
            dynamic_string m_base;     // base path only
            dynamic_string m_rel;      // relative filename
            dynamic_string m_name;     // filename only
            bool m_is_dir;

            inline bool operator==(const file_desc &other) const
            {
                return m_fullname == other.m_fullname;
            }
            inline bool operator<(const file_desc &other) const
            {
                return m_fullname < other.m_fullname;
            }

            inline operator size_t() const
            {
                return m_fullname.get_hash();
            }
        };

        typedef vogl::vector<file_desc> file_desc_vec;

        inline find_files()
        {
            m_last_error = 0; // S_OK;
        }

        enum flags
        {
            cFlagRecursive = 1,
            cFlagAllowDirs = 2,
            cFlagAllowFiles = 4,
            cFlagAllowHidden = 8
        };

        bool find(const char *pBasepath, const char *pFilespec, uint32_t flags = cFlagAllowFiles);

        bool find(const char *pSpec, uint32_t flags = cFlagAllowFiles);

        // An HRESULT under Win32. FIXME: Abstract this better?
        inline int64_t get_last_error() const
        {
            return m_last_error;
        }

        const file_desc_vec &get_files() const
        {
            return m_files;
        }

    private:
        file_desc_vec m_files;

        // A HRESULT under Win32
        int64_t m_last_error;

        bool find_internal(const char *pBasepath, const char *pRelpath, const char *pFilespec, uint32_t flags, int level);

    }; // class find_files

} // namespace vogl
