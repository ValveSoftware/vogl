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

// File: vogl_unique_ptr.h
#pragma once

#include "vogl_core.h"

namespace vogl
{
    template <typename T>
    class vogl_unique_ptr_default_delete_policy
    {
    protected:
        void swap(vogl_unique_ptr_default_delete_policy<T> &rhs)
        {
            VOGL_NOTE_UNUSED(rhs);
        }

        void delete_ptr(T *p)
        {
            vogl_delete(p);
        }
    };

    // First attempt at a std::unique_ptr-like object. This shit is subtle so there are probably dragons in here. Note I could care less about exception safety.
    // pointer must be allocated using vogl_new().
    // Using a policy to handle deletion, vs. an object type to avoid this object from growing larger than a ptr in the default case.
    template <typename T, typename D = vogl_unique_ptr_default_delete_policy<T> >
    class vogl_unique_ptr : public utils::relative_ops<vogl_unique_ptr<T, D> >, public D
    {
    public:
        typedef T element_type;
        typedef T *pointer;
        typedef D delete_policy;

        vogl_unique_ptr()
            : m_p()
        {
        }

        vogl_unique_ptr(pointer p)
            : m_p(p)
        {
        }

        vogl_unique_ptr(vogl_unique_ptr &other)
            : m_p(other.release())
        {
        }

        ~vogl_unique_ptr()
        {
            reset();
        }

        void reset(pointer p = pointer())
        {
            VOGL_ASSUME(sizeof(T) > 0);
            VOGL_ASSERT((!p) || (p != m_p));

            pointer prev_ptr = m_p;
            m_p = p;
            if (prev_ptr)
                delete_policy::delete_ptr(prev_ptr);
        }

        pointer release()
        {
            pointer p = m_p;
            m_p = NULL;
            return p;
        }

        vogl_unique_ptr &operator=(vogl_unique_ptr &rhs)
        {
            if (this != &rhs)
            {
                reset(rhs.release());
            }
            return *this;
        }

        void swap(vogl_unique_ptr &other)
        {
            std::swap(m_p, other.m_p);
            delete_policy::swap(other);
        }

#if 0
	// Feels too dangerous without explicit, but we need C++0x for that.
	explicit operator bool() const
	{
		return m_p != NULL;
	}
#endif

        pointer get() const
        {
            return m_p;
        }

        pointer operator->() const
        {
            return m_p;
        }

        T &operator*() const
        {
            return *m_p;
        }

        bool operator==(const vogl_unique_ptr &rhs) const
        {
            return m_p == rhs.m_p;
        }

        bool operator<(const vogl_unique_ptr &rhs) const
        {
            return m_p < rhs.m_p;
        }

    private:
        pointer m_p;
    };
}

namespace std
{
    template <typename T>
    inline void swap(vogl::vogl_unique_ptr<T> &a, vogl::vogl_unique_ptr<T> &b)
    {
        a.swap(b);
    }
}
