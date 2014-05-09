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

// File: vogl_threading_null.h
#pragma once

#include "vogl_core.h"
#include "vogl_atomics.h"

namespace vogl
{
    const uint32_t g_number_of_processors = 1;

    inline void vogl_threading_init()
    {
    }

    typedef uint64_t vogl_thread_id_t;
    inline vogl_thread_id_t vogl_get_current_thread_id()
    {
        return 0;
    }

    inline void vogl_sleep(unsigned int milliseconds)
    {
        VOGL_NOTE_UNUSED(milliseconds);
    }

    inline uint32_t vogl_get_max_helper_threads()
    {
        return 0;
    }

    class mutex
    {
        VOGL_NO_COPY_OR_ASSIGNMENT_OP(mutex);

    public:
        inline mutex(unsigned int spin_count = 0)
        {
            VOGL_NOTE_UNUSED(spin_count);
        }

        inline ~mutex()
        {
        }

        inline void lock()
        {
        }

        inline void unlock()
        {
        }

        inline void set_spin_count(unsigned int count)
        {
            VOGL_NOTE_UNUSED(count);
        }
    };

    class scoped_mutex
    {
        scoped_mutex(const scoped_mutex &);
        scoped_mutex &operator=(const scoped_mutex &);

    public:
        inline scoped_mutex(mutex &lock)
            : m_lock(lock)
        {
            m_lock.lock();
        }
        inline ~scoped_mutex()
        {
            m_lock.unlock();
        }

    private:
        mutex &m_lock;
    };

    // Simple non-recursive spinlock.
    class spinlock
    {
    public:
        inline spinlock()
        {
        }

        inline void lock(uint32_t max_spins = 4096, bool yielding = true, bool memoryBarrier = true)
        {
            VOGL_NOTE_UNUSED(max_spins), VOGL_NOTE_UNUSED(yielding), VOGL_NOTE_UNUSED(memoryBarrier);
        }

        inline void lock_no_barrier(uint32_t max_spins = 4096, bool yielding = true)
        {
            VOGL_NOTE_UNUSED(max_spins), VOGL_NOTE_UNUSED(yielding);
        }

        inline void unlock()
        {
        }

        inline void unlock_no_barrier()
        {
        }
    };

    class scoped_spinlock
    {
        scoped_spinlock(const scoped_spinlock &);
        scoped_spinlock &operator=(const scoped_spinlock &);

    public:
        inline scoped_spinlock(spinlock &lock)
            : m_lock(lock)
        {
            m_lock.lock();
        }
        inline ~scoped_spinlock()
        {
            m_lock.unlock();
        }

    private:
        spinlock &m_lock;
    };

    class semaphore
    {
        VOGL_NO_COPY_OR_ASSIGNMENT_OP(semaphore);

    public:
        inline semaphore(uint32_t initialCount, uint32_t maximumCount, const char *pName = NULL)
        {
            VOGL_NOTE_UNUSED(initialCount), VOGL_NOTE_UNUSED(maximumCount), VOGL_NOTE_UNUSED(pName);
        }

        inline ~semaphore()
        {
        }

        inline void release(uint32_t releaseCount = 1)
        {
            VOGL_NOTE_UNUSED(releaseCount);
        }

        inline bool wait(uint32_t milliseconds = cUINT32_MAX)
        {
            VOGL_NOTE_UNUSED(milliseconds);
            return true;
        }
    };

    class task_pool
    {
    public:
        inline task_pool()
        {
        }
        inline task_pool(uint32_t num_threads)
        {
            num_threads;
        }
        inline ~task_pool()
        {
        }

        inline bool init(uint32_t num_threads)
        {
            num_threads;
            return true;
        }
        inline void deinit()
        {
        }

        inline uint32_t get_num_threads() const
        {
            return 0;
        }
        inline uint32_t get_num_outstanding_tasks() const
        {
            return 0;
        }

        // C-style task callback
        typedef void (*task_callback_func)(uint64_t data, void *pData_ptr);
        inline bool queue_task(task_callback_func pFunc, uint64_t data = 0, void *pData_ptr = NULL)
        {
            pFunc(data, pData_ptr);
            return true;
        }

        class executable_task
        {
        public:
            virtual void execute_task(uint64_t data, void *pData_ptr) = 0;
        };

        // It's the caller's responsibility to delete pObj within the execute_task() method, if needed!
        inline bool queue_task(executable_task *pObj, uint64_t data = 0, void *pData_ptr = NULL)
        {
            pObj->execute_task(data, pData_ptr);
            return true;
        }

        template <typename S, typename T>
        inline bool queue_object_task(S *pObject, T pObject_method, uint64_t data = 0, void *pData_ptr = NULL)
        {
            (pObject->*pObject_method)(data, pData_ptr);
            return true;
        }

        template <typename S, typename T>
        inline bool queue_multiple_object_tasks(S *pObject, T pObject_method, uint64_t first_data, uint32_t num_tasks, void *pData_ptr = NULL)
        {
            for (uint32_t i = 0; i < num_tasks; i++)
            {
                (pObject->*pObject_method)(first_data + i, pData_ptr);
            }
            return true;
        }

        inline void join()
        {
        }
    };

} // namespace vogl
